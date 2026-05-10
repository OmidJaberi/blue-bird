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

static json_node_t *load_root(const char *path)
{
    json_node_t *root = json_new(JSON_OBJECT);

    if (load_json(root, path) != 0)
    {
        // file might not exist -> start fresh
        destroy_json(root);
        root = json_new(JSON_OBJECT);
    }

    return root;
}

static int save_root(const char *path, json_node_t *root)
{
    int rc = dump_json(root, path);
    destroy_json(root);
    free(root);
    return rc;
}

static json_node_t *get_table(json_node_t *root, const char *name)
{
    json_node_t *table = get_json_object_value(root, name);

    if (!table)
    {
        table = json_new(JSON_ARRAY);
        set_json_object_value(root, name, table);
    }

    return table;
}

static void *get_entity_pk_ptr(BB_Schema *schema, void *entity)
{
    BB_Field *pk = &schema->fields[schema->primary_key_index];
    return (char *)entity + pk->offset;
}

static int pk_equals(BB_Field *pk, json_node_t *node, const void *key)
{
    if (!node) return 0;

    switch (pk->type)
    {
        case BB_FIELD_INT:
            return get_json_integer_value(node) == *(int *)key;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
        {
            char *text = get_json_text_value(node);
            return text && strcmp(text, (const char *)key) == 0;
        }

        case BB_FIELD_BLOB:
            return 0; // not supported in JSON backend
    }

    return 0;
}

// Model Operations:

static BB_ModelHandle *json_open(const char *uri)
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

    return (BB_ModelHandle *)h;
}

static void json_close(BB_ModelHandle *handle)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;
    if (!h) return;

    free(h->path);
    free(h);
}

static int json_insert(BB_ModelHandle *handle, BB_Schema *schema, void *entity)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t *root = load_root(h->path);
    json_node_t *table = get_table(root, schema->name);

    BB_Field *pk = &schema->fields[schema->primary_key_index];
    void *key = get_entity_pk_ptr(schema, entity);

    // check duplicate
    for (unsigned int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);
        json_node_t *id_node = get_json_object_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            destroy_json(root);
            free(root);
            return -1;
        }
    }

    json_node_t *obj = bb_entity_to_json(schema, entity);
    push_json_array(table, obj);

    return save_root(h->path, root);
}

static int json_find_by_pk(BB_ModelHandle *handle, BB_Schema *schema, void *out, const void *key)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t *root = load_root(h->path);
    json_node_t *table = get_table(root, schema->name);

    BB_Field *pk = &schema->fields[schema->primary_key_index];

    for (unsigned int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);
        json_node_t *id_node = get_json_object_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            int rc = bb_json_to_entity(schema, item, out);
            destroy_json(root);
            free(root);
            return rc;
        }
    }

    destroy_json(root);
    free(root);
    return -1;
}

static int json_update(BB_ModelHandle *handle, BB_Schema *schema, void *entity)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t *root = load_root(h->path);
    json_node_t *table = get_table(root, schema->name);

    BB_Field *pk = &schema->fields[schema->primary_key_index];
    void *key = get_entity_pk_ptr(schema, entity);

    for (unsigned int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);
        json_node_t *id_node = get_json_object_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            // remove old entry
            json_array_remove_at_index(table, i);

            // insert updated version
            json_node_t *new_obj = bb_entity_to_json(schema, entity);
            push_json_array(table, new_obj);

            return save_root(h->path, root);
        }
    }

    destroy_json(root);
    free(root);
    return -1;
}

static int json_remove(BB_ModelHandle *handle, BB_Schema *schema, const void *key)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t *root = load_root(h->path);
    json_node_t *table = get_table(root, schema->name);

    BB_Field *pk = &schema->fields[schema->primary_key_index];

    for (unsigned int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);
        json_node_t *id_node = get_json_object_value(item, pk->name);

        if (pk_equals(pk, id_node, key))
        {
            json_array_remove_at_index(table, i);
            return save_root(h->path, root);
        }
    }

    destroy_json(root);
    free(root);
    return -1;
}

static int json_find_all(BB_ModelHandle *handle, BB_Schema *schema, void **out_array, size_t *out_count)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t root;
    init_json(&root, JSON_ARRAY);

    /* load file */
    if (load_json(&root, h->path) != 0)
    {
        destroy_json(&root);

        *out_array = NULL;
        *out_count = 0;

        return 0;
    }

    if (root.type != JSON_OBJECT)
    {
        destroy_json(&root);
        return -1;
    }

    json_node_t *arr = get_json_object_value(&root, schema->name);

    if (!arr)
    {
        *out_array = NULL;
        *out_count = 0;

        destroy_json(&root);
        return 0;
    }

    if (arr->type != JSON_ARRAY)
    {
        destroy_json(&root);
        return -1;
    }

    size_t count = arr->size;

    void *buffer = calloc(count, schema->struct_size);
    if (!buffer)
    {
        destroy_json(&root);
        return -1;
    }

    for (size_t i = 0; i < count; i++)
    {
        json_node_t *obj = get_json_array_index(arr, i);

        if (!obj || obj->type != JSON_OBJECT)
            continue;

        void *entity = (char *)buffer + (i * schema->struct_size);

        bb_json_to_entity(schema, obj, entity);
    }

    destroy_json(&root);

    *out_array = buffer;
    *out_count = count;

    return 0;
}

static int json_find_first_by_field(BB_ModelHandle *handle, BB_Schema *schema, void *out, const char *field_name, const void *value)
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

    BB_Field *field =
        find_field(schema, field_name);

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

static BB_ModelAPI model_json_api = {
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

const BB_ModelAPI *bb_model_json_api(void)
{
    return &model_json_api;
}
