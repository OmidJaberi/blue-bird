#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "blue-bird/utils/json.h"
#include "blue-bird/persist/model/model_json.h"
#include "blue-bird/persist/serialization/entity_json.h"

typedef struct {
    char *path;
} BB_ModelJSONHandle;


// Helpers

static bb_json_t load_root(const char *path)
{
    bb_json_t root = bb_json_create(BB_JSON_OBJECT);

    if (bb_json_load(&root, path) != 0)
    {
        // file might not exist -> start fresh
        bb_json_destroy(root);
        root = bb_json_create(BB_JSON_OBJECT);
    }

    return root;
}

static int save_root(const char *path, bb_json_t root)
{
    int rc = bb_json_dump(root, path);
    bb_json_destroy(root);
    return rc;
}

static bb_json_t get_table(bb_json_t root, const char *name)
{
    bb_json_t table = bb_json_object_get_value(root, name);

    if (!table)
    {
        table = bb_json_create(BB_JSON_ARRAY);
        bb_json_object_set_value(root, name, table);
    }

    return table;
}

static void *get_entity_pk_ptr(bb_schema_t *schema, void *entity)
{
    bb_field_t *pk = &schema->fields[schema->primary_key_index];
    return (char *)entity + pk->offset;
}

static int pk_equals(bb_field_t *pk, bb_json_t node, const void *key)
{
    if (!node) return 0;

    switch (pk->type)
    {
        case BB_FIELD_INT:
            return bb_json_get_value_integer(node) == *(int *)key;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
        {
            char *text = bb_json_get_value_text(node);
            return text && strcmp(text, (const char *)key) == 0;
        }

        case BB_FIELD_BLOB:
            return 0; // not supported in JSON backend
    }

    return 0;
}

// Model Operations:

static bb_model_handle_t *json_open(const char *uri)
{
    if (!uri) return NULL;

    BB_ModelJSONHandle *h = malloc(sizeof(*h));
    if (!h) return NULL;

    h->path = strdup(uri);
    if (!h->path)
    {
        free(h);
        return NULL;
    }

    return (bb_model_handle_t *)h;
}

static void json_close(bb_model_handle_t *handle)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;
    if (!h) return;

    free(h->path);
    free(h);
}

static int json_insert(bb_model_handle_t *handle, bb_schema_t *schema, void *entity)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    bb_json_t root = load_root(h->path);
    bb_json_t table = get_table(root, schema->name);

    bb_field_t *pk = &schema->fields[schema->primary_key_index];
    void *key = get_entity_pk_ptr(schema, entity);

    // check duplicate
    for (unsigned int i = 0; i < bb_json_get_size(table); i++)
    {
        bb_json_t item = bb_json_array_get_index(table, i);
        bb_json_t id_node = bb_json_object_get_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            bb_json_destroy(root);
            return -1;
        }
    }

    bb_json_t obj = bb_entity_to_json(schema, entity);
    bb_json_array_push(table, obj);

    return save_root(h->path, root);
}

static int json_find_by_pk(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const void *key)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    bb_json_t root = load_root(h->path);
    bb_json_t table = get_table(root, schema->name);

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    for (unsigned int i = 0; i < bb_json_get_size(table); i++)
    {
        bb_json_t item = bb_json_array_get_index(table, i);
        bb_json_t id_node = bb_json_object_get_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            int rc = bb_json_to_entity(schema, item, out);
            bb_json_destroy(root);
            return rc;
        }
    }

    bb_json_destroy(root);
    return -1;
}

static int json_update(bb_model_handle_t *handle, bb_schema_t *schema, void *entity)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    bb_json_t root = load_root(h->path);
    bb_json_t table = get_table(root, schema->name);

    bb_field_t *pk = &schema->fields[schema->primary_key_index];
    void *key = get_entity_pk_ptr(schema, entity);

    for (unsigned int i = 0; i < bb_json_get_size(table); i++)
    {
        bb_json_t item = bb_json_array_get_index(table, i);
        bb_json_t id_node = bb_json_object_get_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            // remove old entry
            bb_json_array_remove_at_index(table, i);

            // insert updated version
            bb_json_t new_obj = bb_entity_to_json(schema, entity);
            bb_json_array_push(table, new_obj);

            return save_root(h->path, root);
        }
    }

    bb_json_destroy(root);
    return -1;
}

static int json_remove(bb_model_handle_t *handle, bb_schema_t *schema, const void *key)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    bb_json_t root = load_root(h->path);
    bb_json_t table = get_table(root, schema->name);

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    for (unsigned int i = 0; i < bb_json_get_size(table); i++)
    {
        bb_json_t item = bb_json_array_get_index(table, i);
        bb_json_t id_node = bb_json_object_get_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            bb_json_array_remove_at_index(table, i);
            return save_root(h->path, root);
        }
    }

    bb_json_destroy(root);
    return -1;
}

static int json_find_all(bb_model_handle_t *handle, bb_schema_t *schema, void **out_array, size_t *out_count)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    bb_json_t root = bb_json_create(BB_JSON_ARRAY);

    /* load file */
    if (bb_json_load(&root, h->path) != 0)
    {
        bb_json_destroy(root);

        *out_array = NULL;
        *out_count = 0;

        return 0;
    }

    if (bb_json_get_type(root) != BB_JSON_OBJECT)
    {
        bb_json_destroy(root);
        return -1;
    }

    bb_json_t arr = bb_json_object_get_value(root, schema->name);

    if (!arr)
    {
        *out_array = NULL;
        *out_count = 0;

        bb_json_destroy(root);
        return 0;
    }

    if (bb_json_get_type(arr) != BB_JSON_ARRAY)
    {
        bb_json_destroy(root);
        return -1;
    }

    size_t count = bb_json_get_size(arr);

    void *buffer = calloc(count, schema->struct_size);
    if (!buffer)
    {
        bb_json_destroy(root);
        return -1;
    }

    for (size_t i = 0; i < count; i++)
    {
        bb_json_t obj = bb_json_array_get_index(arr, i);

        if (!obj || bb_json_get_type(obj) != BB_JSON_OBJECT)
            continue;

        void *entity = (char *)buffer + (i * schema->struct_size);

        bb_json_to_entity(schema, obj, entity);
    }

    bb_json_destroy(root);

    *out_array = buffer;
    *out_count = count;

    return 0;
}

static int json_find_first_by_field(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const char *field_name, const void *value)
{
    void *arr = NULL;
    size_t count = 0;

    if (json_find_all(handle,
                      schema,
                      &arr,
                      &count) != 0)
    {
        return -1;
    }

    bb_field_t *field =
        bb_schema_find_field(schema, field_name);

    if (!field)
    {
        free(arr);
        return -1;
    }

    for (size_t i = 0; i < count; i++)
    {
        void *entity =
            (char *)arr + (i * schema->struct_size);

        void *field_ptr =
            (char *)entity + field->offset;

        int match = 0;

        switch (field->type)
        {
            case BB_FIELD_INT:
                match =
                    (*(int *)field_ptr ==
                     *(int *)value);
                break;

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
                match =
                    strcmp((char *)field_ptr,
                           (char *)value) == 0;
                break;

            default:
                break;
        }

        if (match)
        {
            memcpy(out,
                   entity,
                   schema->struct_size);

            free(arr);
            return 0;
        }
    }

    free(arr);

    return -1;
}

static bb_model_api_t model_json_api = {
    .name                = "json",
    .open                = json_open,
    .close               = json_close,
    .insert              = json_insert,
    .find_by_pk          = json_find_by_pk,
    .update              = json_update,
    .remove              = json_remove,
    .find_all            = json_find_all,
    .find_first_by_field = json_find_first_by_field
};

const bb_model_api_t *bb_model_json_api(void)
{
    return &model_json_api;
}
