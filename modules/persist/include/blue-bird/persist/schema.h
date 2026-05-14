#ifndef BB_PERSIST_SCHEMA_H
#define BB_PERSIST_SCHEMA_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <string.h>

typedef enum {
    BB_FIELD_INT,
    BB_FIELD_STRING,
    BB_FIELD_UUID,
    BB_FIELD_BLOB
} bb_field_type_t;

typedef enum {
    BB_FIELD_NONE           = 0,

    BB_FIELD_PRIMARY_KEY    = 1 << 0,
    BB_FIELD_REQUIRED       = 1 << 1,
    BB_FIELD_UNIQUE         = 1 << 2,
    BB_FIELD_AUTO_GENERATE  = 1 << 3
} bb_field_flags_t;

typedef struct {
    const char *name;
    bb_field_type_t type;

    size_t offset;
    size_t size;

    unsigned int flags;

    const char *references_schema;
    const char *references_field;
} bb_field_t;

typedef struct {
    const char *name;
    bb_field_t *fields;
    size_t field_count;
    size_t struct_size;
    unsigned long primary_key_index;
} bb_schema_t;

static inline bb_field_t *bb_schema_find_field(bb_schema_t *schema, const char *name)
{
    for (size_t i = 0; i < schema->field_count; i++)
    {
        if (strcmp(schema->fields[i].name, name) == 0)
            return &schema->fields[i];
    }

    return NULL;
}

int bb_schema_validate(bb_schema_t *schema);
int bb_schema_register(bb_schema_t *schema);
bb_schema_t *bb_schema_get(const char *name);


#ifdef __cplusplus
}
#endif

#endif //BB_PERSIST_SCHEMA_H
