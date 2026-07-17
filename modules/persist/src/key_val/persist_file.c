#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "blue-bird/persist/key_val.h"
#include "blue-bird/persist/key_val/persist_file.h"

struct bb_persist_kv_handle_t {
    char *basepath;
};

static bb_persist_kv_handle_t *file_open(const char *uri)
{
    if (!uri) return NULL;

    bb_persist_kv_handle_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;

    h->basepath = strdup(uri);
    if (!h->basepath) { free(h); return NULL; }

#ifdef _WIN32
    _mkdir(uri);
#else
    mkdir(uri, 0755);
#endif

    return h;
}

static void file_close(bb_persist_kv_handle_t *h)
{
    if (!h) return;
    free(h->basepath);
    free(h);
}

static char *make_path(bb_persist_kv_handle_t *h, const char *key)
{
    size_t len = strlen(h->basepath) + 1 + strlen(key) + 1;
    char *path = malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/%s", h->basepath, key);
    return path;
}

static int file_save(bb_persist_kv_handle_t *h, const char *key,
                     const void *data, size_t size)
{
    if (!h || !key || !data) return 1;

    char *path = make_path(h, key);
    if (!path) return 1;
    FILE *f = fopen(path, "wb");
    free(path);

    if (!f) return 1;

    fwrite(data, 1, size, f);
    fclose(f);
    return 0;
}

static int file_load(bb_persist_kv_handle_t *h, const char *key,
                     void *buf, size_t bufsize)
{
    if (!h || !key || !buf) return 1;

    char *path = make_path(h, key);
    if (!path) return 1;
    FILE *f = fopen(path, "rb");
    free(path);

    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if ((size_t)size > bufsize) {
        fclose(f);
        return 1;
    }

    fread(buf, 1, size, f);
    ((char *)buf)[size] = '\0';
    fclose(f);
    return 0;
}

static int file_remove(bb_persist_kv_handle_t *h, const char *key)
{
    if (!h || !key) return 1;

    char *path = make_path(h, key);
    if (!path) return 1;
    int rc = remove(path);
    free(path);

    return (rc == 0) ? 0 : 1;
}

static const bb_persist_kv_api_t file_api = {
    .name   = "file",
    .open   = file_open,
    .close  = file_close,
    .save   = file_save,
    .load   = file_load,
    .remove = file_remove,
};

int bb_persist_kv_file_register(void)
{
    return bb_persist_kv_register(&file_api);
}
