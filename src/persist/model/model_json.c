#include <stdio.h>

#include "utils/json.h"
#include "persist/model/model_json.h"

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

static int get_entity_id(BB_Schema *schema, void *entity)
{
    BB_Field *pk = &schema->fields[schema->primary_key_index];
    return *(int *)((char *)entity + pk->offset);
}

// Mapping

static json_node_t *entity_to_json(BB_Schema *schema, void *entity)
{
    json_node_t *obj = json_new(JSON_OBJECT);

    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];
        void *field_ptr = (char *)entity + f->offset;

        json_node_t *val = NULL;

        switch (f->type)
        {
            case BB_FIELD_INT:
                val = json_new_int(*(int *)field_ptr);
                break;

            case BB_FIELD_STRING:
                val = json_new_text((char *)field_ptr);
                break;

            case BB_FIELD_BLOB:
                // skip for now or store as null
                val = json_new_null();
                break;
        }

        set_json_object_value(obj, f->name, val);
    }

    return obj;
}

static int json_to_entity(BB_Schema *schema, json_node_t *obj, void *out)
{
    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];
        void *field_ptr = (char *)out + f->offset;

        json_node_t *val = get_json_object_value(obj, f->name);
        if (!val)
            return -1;

        switch (f->type)
        {
            case BB_FIELD_INT:
                *(int *)field_ptr = get_json_integer_value(val);
                break;

            case BB_FIELD_STRING:
                char *text = get_json_text_value(val);
                if (text)
                    snprintf((char *)field_ptr, f->size, "%s", text);
                break;

            case BB_FIELD_BLOB:
                // skip for now
                break;
        }
    }

    return 0;
}

// Model Operations:

static int json_insert(BB_ModelHandle *handle, BB_Schema *schema, void *entity)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t *root = load_root(h->path);
    json_node_t *table = get_table(root, schema->name);

    int id = get_entity_id(schema, entity);

    // check duplicate
    for (int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);
        json_node_t *id_node = get_json_object_value(item, schema->fields[schema->primary_key_index].name);

        if (id_node && get_json_integer_value(id_node) == id)
        {
            destroy_json(root);
            free(root);
            return -1;
        }
    }

    json_node_t *obj = entity_to_json(schema, entity);
    push_json_array(table, obj);

    return save_root(h->path, root);
}

static int json_find_by_id(BB_ModelHandle *handle, BB_Schema *schema, void *out, int id)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t *root = load_root(h->path);
    json_node_t *table = get_table(root, schema->name);

    for (int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);

        json_node_t *id_node = get_json_object_value(
            item, schema->fields[schema->primary_key_index].name
        );

        if (id_node && get_json_integer_value(id_node) == id)
        {
            int rc = json_to_entity(schema, item, out);
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

    int id = get_entity_id(schema, entity);
    const char *pk_name = schema->fields[schema->primary_key_index].name;

    for (int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);
        json_node_t *id_node = get_json_object_value(item, pk_name);

        if (id_node && get_json_integer_value(id_node) == id)
        {
            // remove old entry
            json_array_remove_at_index(table, i);

            // insert updated version
            json_node_t *new_obj = entity_to_json(schema, entity);
            push_json_array(table, new_obj);

            return save_root(h->path, root);
        }
    }

    destroy_json(root);
    free(root);
    return -1;
}

static int json_remove(BB_ModelHandle *handle, BB_Schema *schema, int id)
{
    BB_ModelJSONHandle *h = (BB_ModelJSONHandle *)handle;

    json_node_t *root = load_root(h->path);
    json_node_t *table = get_table(root, schema->name);

    const char *pk_name = schema->fields[schema->primary_key_index].name;

    for (int i = 0; i < table->size; i++)
    {
        json_node_t *item = get_json_array_index(table, i);
        json_node_t *id_node = get_json_object_value(item, pk_name);

        if (id_node && get_json_integer_value(id_node) == id)
        {
            json_array_remove_at_index(table, i);
            return save_root(h->path, root);
        }
    }

    destroy_json(root);
    free(root);
    return -1;
}