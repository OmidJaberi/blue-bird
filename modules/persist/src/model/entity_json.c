#include "blue-bird/persist/entity_json.h"

#include <string.h>

json_node_t *bb_entity_to_json(BB_Schema *schema, void *entity)
{
    if (!schema || !entity)
        return NULL;

    json_node_t *obj = json_new(JSON_OBJECT);

    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];

        void *field_ptr = (char *)entity + f->offset;

        json_node_t *val = NULL;

        switch (f->type)
        {
            case BB_FIELD_INT:
            {
                val = json_new_int(
                    *(int *)field_ptr
                );
                break;
            }

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
            {
                val = json_new_text(
                    (char *)field_ptr
                );
                break;
            }

            case BB_FIELD_BLOB:
            {
                /* unsupported for now */
                break;
            }

            default:
                break;
        }

        if (val)
        {
            set_json_object_value(obj, f->name, val);
        }
    }

    return obj;
}

int bb_json_to_entity(BB_Schema *schema, json_node_t *json, void *out)
{
    if (!schema || !json || !out)
        return -1;

    if (json->type != JSON_OBJECT)
        return -1;

    memset(out, 0, schema->struct_size);

    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];

        json_node_t *val = get_json_object_value(json, f->name);

        if (!val)
            continue;

        void *field_ptr = (char *)out + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
            {
                if (val->type != JSON_INT)
                    return -1;

                *(int *)field_ptr = get_json_integer_value(val);

                break;
            }

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
            {
                if (val->type != JSON_TEXT)
                    return -1;

                strncpy((char *)field_ptr, get_json_text_value(val), f->size);

                break;
            }

            case BB_FIELD_BLOB:
            {
                /* unsupported for now */
                break;
            }

            default:
                return -1;
        }
    }

    return 0;
}