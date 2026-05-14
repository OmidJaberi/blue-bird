#include "blue-bird/persist/schema.h"

#include <string.h>

#define BB_MAX_SCHEMAS 256

static bb_schema_t *g_schemas[BB_MAX_SCHEMAS];
static size_t g_schema_count = 0;

bb_schema_t *bb_schema_get(const char *name)
{
    if (!name)
        return NULL;

    for (size_t i = 0; i < g_schema_count; i++)
    {
        bb_schema_t *s = g_schemas[i];

        if (strcmp(s->name, name) == 0)
            return s;
    }

    return NULL;
}

int bb_schema_register(bb_schema_t *schema)
{
    if (!schema)
        return -1;

    /* validate schema first */
    if (bb_schema_validate(schema) != 0)
        return -1;

    /* duplicate schema name */
    if (bb_schema_get(schema->name))
        return -1;

    if (g_schema_count >= BB_MAX_SCHEMAS)
        return -1;

    g_schemas[g_schema_count++] = schema;

    return 0;
}

int bb_schema_validate(bb_schema_t *schema)
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
        bb_field_t *f = &schema->fields[i];

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
            bb_field_t *other = &schema->fields[j];

            if (strcmp(f->name, other->name) == 0)
                return -1;
        }

        /* relationship metadata sanity */
        if (f->references_schema && !f->references_field)
            return -1;

        if (!f->references_schema && f->references_field)
            return -1;

        if (f->references_schema)
        {
            bb_schema_t *ref_schema = bb_schema_get(f->references_schema);

            if (!ref_schema)
                return -1;

            int found = 0;

            for (size_t k = 0; k < ref_schema->field_count; k++)
            {
                if (strcmp(ref_schema->fields[k].name, f->references_field) == 0)
                {
                    found = 1;
                    break;
                }
            }

            if (!found)
                return -1;
        }
    }

    return 0;
}
