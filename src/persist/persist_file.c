#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "persist.h"
#include "persist_file.h"

struct PersistHandle {
    char *basepath;
};

static PersistHandle *file_open(const char *uri)
{
    if (!uri) return NULL;

    PersistHandle *h = calloc(1, sizeof(*h));
    if (!h) return NULL;

    h->basepath = strdup(uri);

#ifdef _WIN32
    _mkdir(uri);
#else
    mkdir(uri, 0755);
#endif

    return h;
}

static void file_close(PersistHandle *h)
{
    if (!h) return;
    free(h->basepath);
    free(h);
}

static char *make_path(PersistHandle *h, const char *key)
{
    size_t len = strlen(h->basepath) + 1 + strlen(key) + 1;
    char *path = malloc(len);
    snprintf(path, len, "%s/%s", h->basepath, key);
    return path;
}

static int file_save(PersistHandle *h, const char *key,
                     const void *data, size_t size)
{
    if (!h || !key || !data) return 1;

    char *path = make_path(h, key);
    FILE *f = fopen(path, "wb");
    free(path);

    if (!f) return 1;

    fwrite(data, 1, size, f);
    fclose(f);
    return 0;
}

static int file_load(PersistHandle *h, const char *key,
                     void *buf, size_t bufsize)
{
    if (!h || !key || !buf) return 1;

    char *path = make_path(h, key);
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
    fclose(f);
    return 0;
}

static int file_remove(PersistHandle *h, const char *key)
{
    if (!h || !key) return 1;

    char *path = make_path(h, key);
    int rc = remove(path);
    free(path);

    return (rc == 0) ? 0 : 1;
}

static const PersistAPI file_api = {
    .name   = "file",
    .open   = file_open,
    .close  = file_close,
    .save   = file_save,
    .load   = file_load,
    .remove = file_remove,
};

int persist_file_register(void)
{
    return persist_register(&file_api);
}
