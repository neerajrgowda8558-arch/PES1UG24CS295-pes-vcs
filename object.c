#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "pes.h"   // defines ObjectID, object_path, compute_hash

// Write an object (blob/tree/commit) into the store
int object_write(const char *type, const void *data, size_t size, ObjectID *id_out) {
    // Build header: "<type> <size>\0"
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type, size);
    if (header_len < 0 || header_len >= (int)sizeof(header)) return -1;

    size_t total_size = header_len + 1 + size;
    char *buffer = malloc(total_size);
    if (!buffer) return -1;

    memcpy(buffer, header, header_len);
    buffer[header_len] = '\0';
    memcpy(buffer + header_len + 1, data, size);

    // Compute SHA-256 of full object
    compute_hash(buffer, total_size, id_out);

    // Build path
    char path[256];
    object_path(id_out, path, sizeof(path));

    // Ensure directories exist
    mkdir(".pes", 0755);
    mkdir(".pes/objects", 0755);

    char dir[256];
    snprintf(dir, sizeof(dir), ".pes/objects/%.2s", id_out->hex);
    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        free(buffer);
        return -1;
    }

    // Atomic write: temp file then rename
    char tmp_path[300];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        free(buffer);
        return -1;
    }

    size_t written = fwrite(buffer, 1, total_size, f);
    if (written != total_size) {
        fclose(f);
        unlink(tmp_path);
        free(buffer);
        return -1;
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(tmp_path, path) < 0) {
        unlink(tmp_path);
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}

// Read an object back from the store
int object_read(const ObjectID *id, char **data_out, size_t *size_out) {
    char path[256];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    if (file_size <= 0) {
        fclose(f);
        return -1;
    }

    char *buffer = malloc(file_size);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    size_t read = fread(buffer, 1, file_size, f);
    fclose(f);
    if (read != (size_t)file_size) {
        free(buffer);
        return -1;
    }

    // Verify hash matches filename
    ObjectID check;
    compute_hash(buffer, file_size, &check);
    if (strcmp(check.hex, id->hex) != 0) {
        free(buffer);
        return -1; // corrupted object
    }

    // Find header terminator
    char *data_start = memchr(buffer, '\0', file_size);
    if (!data_start) {
        free(buffer);
        return -1;
    }

    data_start++;
    size_t data_size = file_size - (data_start - buffer);

    *data_out = malloc(data_size);
    if (!*data_out) {
        free(buffer);
        return -1;
    }
    memcpy(*data_out, data_start, data_size);
    *size_out = data_size;

    free(buffer);
    return 0;
}
