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