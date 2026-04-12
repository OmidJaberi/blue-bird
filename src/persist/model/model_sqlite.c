#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "persist/model/model_sqlite.h"

typedef struct {
    sqlite3 *db;
} BB_ModelSQLiteHandle;

static const char *field_type_to_sql(BB_FieldType type)
{
    switch (type)
    {
        case BB_FIELD_INT: return "INTEGER";
        case BB_FIELD_STRING: return "TEXT";
        case BB_FIELD_BLOB: return "BLOB";
        default: return "BLOB";
    }
}