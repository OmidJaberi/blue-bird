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