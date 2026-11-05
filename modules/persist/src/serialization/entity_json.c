#include "blue-bird/persist/serialization/entity_json.h"

#include <string.h>

bb_json_t *bb_entity_to_json(bb_schema_t *schema, void *entity)
{
    if (!schema || !entity)
        return NULL;

    bb_json_t *obj = bb_json_new_object();

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];

        void *field_ptr = (char *)entity + f->offset;

        bb_json_t *val = NULL;

        switch (f->type)
        {
            case BB_FIELD_INT:
            {
                val = bb_json_new_int(
                    *(int *)field_ptr
                );
                break;
            }

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
            {
                val = bb_json_new_text(
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
            bb_json_object_set_value(obj, f->name, val);
        }
    }

    return obj;
}

/* Copies a JSON string into a fixed-size char field. The field is always
 * NUL-terminated: a value that does not fit is truncated to size - 1 bytes,
 * without cutting a UTF-8 sequence in half. (strncpy() would leave a value of
 * exactly `size` bytes or more unterminated.) */
static void copy_text_field(char *dst, size_t size, const char *text)
{
    if (size == 0)
        return;

    size_t len = text ? strlen(text) : 0;
    size_t n = len < size ? len : size - 1;

    if (n < len)
    {
        /* text[n] is the first byte left out; if it is a UTF-8 continuation
         * byte the cut is inside a character, so step back to its start. */
        while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80)
            n--;
    }

    if (n > 0)
        memcpy(dst, text, n);
    dst[n] = '\0';
}

int bb_json_to_entity(bb_schema_t *schema, bb_json_t *json, void *out)
{
    if (!schema || !json || !out)
        return -1;

    if (bb_json_get_type(json) != BB_JSON_OBJECT)
        return -1;

    memset(out, 0, schema->struct_size);

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];

        bb_json_t *val = bb_json_object_get_value(json, f->name);

        if (!val)
            continue;

        void *field_ptr = (char *)out + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
            {
                if (bb_json_get_type(val) != BB_JSON_INT)
                    return -1;

                *(int *)field_ptr = bb_json_get_value_integer(val);

                break;
            }

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
            {
                if (bb_json_get_type(val) != BB_JSON_TEXT)
                    return -1;

                copy_text_field((char *)field_ptr, f->size, bb_json_get_value_text(val));

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
