#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "blue-bird/persist/key_val.h"
#include "blue-bird/persist/key_val/persist_json.h"
#include "blue-bird/utils/json.h"

struct bb_persist_kv_handle_t {
    char *jsonpath;
    bb_json_t *json;
};

static bb_persist_kv_handle_t *json_open(const char *uri)
{
    if (!uri) return NULL;

    bb_persist_kv_handle_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;

    h->jsonpath = strdup(uri);
    h->json = bb_json_create(BB_JSON_OBJECT);

    return h;
}

static void json_close(bb_persist_kv_handle_t *h)
{
    if (!h) return;
    bb_json_destroy(h->json);
    free(h->jsonpath);
    free(h);
}

static int json_save(bb_persist_kv_handle_t *h, const char *key,
                     const void *data, size_t size)
{
    (void) size;
    if (!h || !key || !data) return 1;

    bb_json_load(&h->json, h->jsonpath);
    bb_json_t *value = bb_json_create(BB_JSON_TEXT);
    bb_json_set_value_text(value, data);
    bb_json_object_set_value(h->json, key, value); // non str data?
    bb_json_dump(h->json, h->jsonpath);

    return 0;
}

static int json_load(bb_persist_kv_handle_t *h, const char *key,
                     void *buf, size_t bufsize)
{
    (void) bufsize;

    if (!h || !key || !buf) return 1;

    bb_json_load(&h->json, h->jsonpath);
    bb_json_t *val_json = bb_json_object_get_value(h->json, key);
    if (val_json)
    {
        char *val = bb_json_get_value_text(val_json);
        memcpy(buf, val, bb_json_get_size(val_json));    
        return 0;
    }
    return -1;
}

static int json_remove(bb_persist_kv_handle_t *h, const char *key)
{
    if (!h || !key) return 1;
    bb_json_load(&h->json, h->jsonpath);
    bb_json_object_remove_key(h->json, key);
    bb_json_dump(h->json, h->jsonpath);
    return 0;
}

static const bb_persist_kv_api_t file_api = {
    .name   = "json",
    .open   = json_open,
    .close  = json_close,
    .save   = json_save,
    .load   = json_load,
    .remove = json_remove,
};

int bb_persist_kv_json_register(void)
{
    return bb_persist_kv_register(&file_api);
}
