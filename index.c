// index.c — Staging area implementation
//
// Text format of .pes/index (one entry per line, sorted by path):
//
//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>
//
// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c
//
// This is intentionally a simple text format. No magic numbers, no
// binary parsing. The focus is on the staging area CONCEPT (tracking
// what will go into the next commit) and ATOMIC WRITES (temp+rename).
//
// PROVIDED functions: index_find, index_remove, index_status
// TODO functions:     index_load, index_save, index_add

#include "index.h"
#include "pes.h"   /* This file contains your ObjectID and ObjectType definitions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

// Forward declaration to fix implicit function warnings
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// ─── PROVIDED FUNCTIONS ──────────────────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    if (!index || !path) return NULL;
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (index->count == 0) {
        printf("  (nothing to show)\n");
    } else {
        for (int i = 0; i < index->count; i++) {
            printf("  staged:     %s\n", index->entries[i].path);
        }
    }
    return 0;
}

// ─── TODO: IMPLEMENTED FUNCTIONS ─────────────────────────────────────────────

int index_load(Index *index) {
    // CRITICAL: Initialize count first to avoid garbage values
    index->count = 0;
    
    FILE *f = fopen(".pes/index", "r");
    if (!f) return 0; // Success even if file doesn't exist yet

    char line[1024];
    while (fgets(line, sizeof(line), f) && index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        char hash_hex[HASH_HEX_SIZE + 1];

        // Using %u for size to match uint32_t precisely
        if (sscanf(line, "%o %64s %ld %u %511s", 
                   &e->mode, hash_hex, &e->mtime_sec, &e->size, e->path) == 5) {
            hex_to_hash(hash_hex, &e->hash);
            index->count++;
        }
    }
    fclose(f);
    return 0;
}

int index_save(const Index *index) {
    // Atomic save: write to .tmp then rename
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;

    for (int i = 0; i < index->count; i++) {
        const IndexEntry *e = &index->entries[i];
        char hash_hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&e->hash, hash_hex);
        
        fprintf(f, "%o %s %ld %u %s\n", 
                e->mode, hash_hex, (long)e->mtime_sec, e->size, e->path);
    }

    fclose(f);
    return rename(".pes/index.tmp", ".pes/index");
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "error: cannot stat '%s'\n", path);
        return -1;
    }

    // 1. Read file into memory for hashing
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    void *data = malloc(st.st_size);
    if (st.st_size > 0 && fread(data, 1, st.st_size, f) != (size_t)st.st_size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);

    // 2. Hash and write the BLOB object
    ObjectID id;
    if (object_write(OBJ_BLOB, data, st.st_size, &id) != 0) {
        free(data);
        return -1;
    }
    free(data);

    // 3. Update or Add entry in Index struct
    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
        strncpy(e->path, path, 511);
        e->path[511] = '\0';
    }

    e->mode = st.st_mode;
    e->hash = id;
    e->mtime_sec = (long)st.st_mtime;
    e->size = (uint32_t)st.st_size;

    return index_save(index);
}
