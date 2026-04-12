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

static int ensure_table(sqlite3 *db, BB_Schema *schema) {
    char sql[1024] = {0};
    strcat(sql, "CREATE TABLE IF NOT EXISTS ");
    strcat(sql, schema->name);
    strcat(sql, " (");

    for (size_t i = 0; i < schema->field_count; i++) {
        BB_Field *f = &schema->fields[i];

        strcat(sql, f->name);
        strcat(sql, " ");
        strcat(sql, field_type_to_sql(f->type));

        if ((int)i == schema->primary_key_index) {
            strcat(sql, " PRIMARY KEY");
        }

        if (i < schema->field_count - 1) {
            strcat(sql, ", ");
        }
    }

    strcat(sql, ");");

    return sqlite3_exec(db, sql, NULL, NULL, NULL);
}