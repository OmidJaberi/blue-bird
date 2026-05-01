#ifndef BB_PERSIST_SCHEMA_H
#define BB_PERSIST_SCHEMA_H

#include <stddef.h>

typedef enum {
    BB_FIELD_INT,
    BB_FIELD_STRING,
    BB_FIELD_UUID,
    BB_FIELD_BLOB
} BB_FieldType;

typedef struct {
    const char *name;
    BB_FieldType type;
    size_t offset;
    size_t size;

    /* relationship metadata (optional) */
    const char *references_schema;
    const char *references_field;
} BB_Field;

typedef struct {
    const char *name;
    BB_Field *fields;
    size_t field_count;
    size_t struct_size;
    int primary_key_index;
} BB_Schema;

#endif //BB_PERSIST_SCHEMA_H
