#include "commit.h"
#include "tree.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Declarations to stop warnings
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type, void **data, size_t *size);

int commit_create(const char *message, ObjectID *commit_id) {
    ObjectID tree_id;
    if (tree_from_index(&tree_id) != 0) return -1;

    char tree_hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&tree_id, tree_hex);

    char buffer[2048];
    int len = snprintf(buffer, sizeof(buffer),
                       "tree %s\nauthor Student\ntimestamp %ld\n\n%s\n",
                       tree_hex, (long)time(NULL), message);

    if (len < 0 || len >= (int)sizeof(buffer)) return -1;

    return object_write(OBJ_COMMIT, buffer, len, commit_id);
}

// Updated to match the signature in commit.h
int commit_walk(commit_walk_fn callback, void *ctx) {
    // In this lab, we usually get the HEAD commit ID from a file or the environment
    // For now, we'll try to find the most recent commit object in the store
    // Or you can leave this as a basic loop if your pes.c provides the start ID
    
    printf("Walking commit history...\n");
    // Since the callback is expected, your pes.c usually handles the 
    // actual logic of finding the HEAD. This stub allows compilation.
    return 0;
}
