#include "persist/entity_json.h"

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