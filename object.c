#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/sha.h>

int object_write(const char *type, const void *data, size_t size, ObjectID *id_out) {
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type, size);

    size_t total_size = header_len + 1 + size;
    char *buffer = malloc(total_size);
    if (!buffer) return -1;

    memcpy(buffer, header, header_len);
    buffer[header_len] = '\0';
    memcpy(buffer + header_len + 1, data, size);

    compute_hash(buffer, total_size, id_out);

    char path[256];
    object_path(id_out, path, sizeof(path));

    mkdir(".pes", 0755);
    mkdir(".pes/objects", 0755);

    char dir[256];
    snprintf(dir, sizeof(dir), ".pes/objects/%.2s", id_out->hex);
    mkdir(dir, 0755);

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(buffer);
        return -1;
    }

    fwrite(buffer, 1, total_size, f);
    fclose(f);

    free(buffer);
    return 0;
}

int object_read(const ObjectID *id, char **data_out, size_t *size_out) {
    char path[256];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    char *buffer = malloc(file_size);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    fread(buffer, 1, file_size, f);
    fclose(f);

    char *data_start = memchr(buffer, '\0', file_size);
    if (!data_start) {
        free(buffer);
        return -1;
    }

    data_start++;
    size_t data_size = file_size - (data_start - buffer);

    *data_out = malloc(data_size);
    memcpy(*data_out, data_start, data_size);
    *size_out = data_size;

    free(buffer);
    return 0;
}
