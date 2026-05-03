#include "persist/schema.h"

#include <string.h>

int bb_schema_validate(BB_Schema *schema)
{
    if (!schema)
        return -1;

    /* schema name */
    if (!schema->name || schema->name[0] == '\0')
        return -1;

    /* fields */
    if (!schema->fields)
        return -1;

    if (schema->field_count == 0)
        return -1;

    /* struct size */
    if (schema->struct_size == 0)
        return -1;

    /* primary key */
    if (schema->primary_key_index >= schema->field_count)
        return -1;

    /* validate fields */
    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];

        /* field name */
        if (!f->name || f->name[0] == '\0')
            return -1;

        /* field size */
        if (f->size == 0)
            return -1;

        /* supported type */
        switch (f->type)
        {
            case BB_FIELD_INT:
            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
            case BB_FIELD_BLOB:
                break;

            default:
                return -1;
        }

        /* duplicate field names */
        for (size_t j = i + 1; j < schema->field_count; j++)
        {
            BB_Field *other = &schema->fields[j];

            if (strcmp(f->name, other->name) == 0)
                return -1;
        }

        /* relationship metadata sanity */
        if (f->references_schema && !f->references_field)
            return -1;

        if (!f->references_schema && f->references_field)
            return -1;
    }

    return 0;
}