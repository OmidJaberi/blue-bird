#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "blue-bird/persist/key_val.h"
#include "blue-bird/persist/key_val/persist_json.h"
#include "blue-bird/utils/json.h"

struct bb_persist_kv_handle_t {
    char *jsonpath;
    json_node_t json;
};

static bb_persist_kv_handle_t *json_open(const char *uri)
{
    if (!uri) return NULL;

    bb_persist_kv_handle_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;

    h->jsonpath = strdup(uri);
    init_json(&h->json, JSON_OBJECT);

    return h;
}

static void json_close(bb_persist_kv_handle_t *h)
{
    if (!h) return;
    destroy_json(&h->json);
    free(h->jsonpath);
    free(h);
}

static int json_save(bb_persist_kv_handle_t *h, const char *key,
                     const void *data, size_t size)
{
    (void) size;
    if (!h || !key || !data) return 1;

    load_json(&h->json, h->jsonpath);
    json_node_t *value = (json_node_t*)malloc(sizeof(json_node_t));
    init_json(value, JSON_TEXT);
    set_json_text_value(value, data);
    set_json_object_value(&h->json, key, value); // non str data?
    dump_json(&h->json, h->jsonpath);

    return 0;
}

static int json_load(bb_persist_kv_handle_t *h, const char *key,
                     void *buf, size_t bufsize)
{
    (void) bufsize;

    if (!h || !key || !buf) return 1;

    load_json(&h->json, h->jsonpath);
    json_node_t *val_json = get_json_object_value(&h->json, key);
    if (val_json)
    {
        char *val = get_json_text_value(val_json);
        memcpy(buf, val, val_json->size);    
        return 0;
    }
    return -1;
}

static int json_remove(bb_persist_kv_handle_t *h, const char *key)
{
    if (!h || !key) return 1;
    load_json(&h->json, h->jsonpath);
    remove_json_object_value(&h->json, key);
    dump_json(&h->json, h->jsonpath);
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
