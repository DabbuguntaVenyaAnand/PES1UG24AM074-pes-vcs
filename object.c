// object.c — Content-addressable object store
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/evp.h>

// ─── PROVIDED FUNCTIONS ──────────────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data1, size_t len1, const void *data2, size_t len2, ObjectID *id_out) {
    unsigned int hash_len;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data1, len1);
    if (data2 && len2 > 0) {
        EVP_DigestUpdate(ctx, data2, len2);
    }
    EVP_DigestFinal_ex(ctx, id_out->hash, &hash_len);
    EVP_MD_CTX_free(ctx);
}

void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─── TODO: IMPLEMENTATIONS ───────────────────────────────────────────────────

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    const char *type_str;
    if (type == OBJ_BLOB) type_str = "blob";
    else if (type == OBJ_TREE) type_str = "tree";
    else if (type == OBJ_COMMIT) type_str = "commit";
    else return -1;

    // 1. Build header
    char header[64];
    int header_len = sprintf(header, "%s %zu", type_str, len) + 1;

    // 2. Compute hash of header + data
    compute_hash(header, header_len, data, len, id_out);

    // 3. Deduplication
    if (object_exists(id_out)) return 0;

    // 4. Create shard directory
    char path[512];
    object_path(id_out, path, sizeof(path));
   
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id_out, hex);
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%.2s", OBJECTS_DIR, hex);
    mkdir(dir_path, 0755);

    // 5. Atomic write using temp file
    char tmp_path[520];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    int fd = open(tmp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) return -1;

    if (write(fd, header, header_len) != header_len ||
        write(fd, data, len) != (ssize_t)len) {
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    fsync(fd);
    close(fd);

    // 6. Rename to final path
    if (rename(tmp_path, path) != 0) return -1;

    // 7. Persist directory
    int dir_fd = open(dir_path, O_RDONLY | O_DIRECTORY);
    if (dir_fd >= 0) {
        fsync(dir_fd);
        close(dir_fd);
    }

    return 0;
}
//commit 2 , made changes in object_read
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    char path[512];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    uint8_t *full_data = malloc(file_size);
    if (!full_data) {
        fclose(f);
        return -1;
    }

    if (fread(full_data, 1, file_size, f) != file_size) {
        fclose(f);
        free(full_data);
        return -1;
    }
    fclose(f);

    // Verify Integrity
    uint8_t *null_pos = memchr(full_data, '\0', file_size);
    if (!null_pos) {
        free(full_data);
        return -1;
    }
    size_t header_len = (null_pos - full_data) + 1;
    size_t data_len = file_size - header_len;

    ObjectID actual_id;
    compute_hash(full_data, header_len, full_data + header_len, data_len, &actual_id);
    if (memcmp(id->hash, actual_id.hash, HASH_SIZE) != 0) {
        free(full_data);
        return -1;
    }

    // Parse Type
    if (strncmp((char*)full_data, "blob", 4) == 0) *type_out = OBJ_BLOB;
    else if (strncmp((char*)full_data, "tree", 4) == 0) *type_out = OBJ_TREE;
    else if (strncmp((char*)full_data, "commit", 6) == 0) *type_out = OBJ_COMMIT;
    else {
        free(full_data);
        return -1;
    }

    // Parse Size
    if (sscanf((char*)full_data, "%*s %zu", len_out) != 1) {
        free(full_data);
        return -1;
    }

    // Copy Data
    *data_out = malloc(*len_out);
    if (!*data_out) {
        free(full_data);
        return -1;
    }
    memcpy(*data_out, full_data + header_len, *len_out);

    free(full_data);
    return 0;
}
