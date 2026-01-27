#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "persist/persist.h"
#include "persist/persist_json.h"
#include "utils/json.h"

struct PersistHandle {
    char *jsonpath;
    json_node_t json;
};

static PersistHandle *json_open(const char *uri)
{
    if (!uri) return NULL;

    PersistHandle *h = calloc(1, sizeof(*h));
    if (!h) return NULL;

    h->jsonpath = strdup(uri);
    init_json(&h->json, object);

    return h;
}

static void json_close(PersistHandle *h)
{
    if (!h) return;
    destroy_json(&h->json);
    free(h->jsonpath);
    free(h);
}

static int json_save(PersistHandle *h, const char *key,
                     const void *data, size_t size)
{
    if (!h || !key || !data) return 1;

    load_json(&h->json, h->jsonpath);
    json_node_t *value = (json_node_t*)malloc(sizeof(json_node_t));
    init_json(value, text);
    set_json_text_value(value, data);
    set_json_object_value(&h->json, key, value); // non str data?
    dump_json(&h->json, h->jsonpath);

    return 0;
}

static int json_load(PersistHandle *h, const char *key,
                     void *buf, size_t bufsize)
{
    if (!h || !key || !buf) return 1;

    load_json(&h->json, h->jsonpath);
    json_node_t *val_json = get_json_object_value(&h->json, key);
    char *val = get_json_text_value(val_json);
    memcpy(buf, val, val_json->size);

    return 0;
}

static int json_remove(PersistHandle *h, const char *key)
{
    if (!h || !key) return 1;

    // Should be implemented in utils/json

    return 0;
}

static const PersistAPI file_api = {
    .name   = "json",
    .open   = json_open,
    .close  = json_close,
    .save   = json_save,
    .load   = json_load,
    .remove = json_remove,
};

int persist_json_register(void)
{
    return persist_register(&file_api);
}
