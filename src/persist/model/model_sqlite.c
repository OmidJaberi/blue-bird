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

static int ensure_table(sqlite3 *db, BB_Schema *schema)
{
    char sql[1024] = {0};
    strcat(sql, "CREATE TABLE IF NOT EXISTS ");
    strcat(sql, schema->name);
    strcat(sql, " (");

    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];

        strcat(sql, f->name);
        strcat(sql, " ");
        strcat(sql, field_type_to_sql(f->type));

        if ((int)i == schema->primary_key_index)
            strcat(sql, " PRIMARY KEY");

        if (i < schema->field_count - 1)
            strcat(sql, ", ");
    }

    strcat(sql, ");");

    return sqlite3_exec(db, sql, NULL, NULL, NULL);
}

static BB_ModelHandle *sqlite_open(const char *uri)
{
    BB_ModelSQLiteHandle *h = malloc(sizeof(*h));
    if (!h) return NULL;

    if (sqlite3_open(uri, &h->db) != SQLITE_OK)
    {
        free(h);
        return NULL;
    }

    return (BB_ModelHandle *)h;
}

static void sqlite_close(BB_ModelHandle *handle)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    if (!h) return;

    sqlite3_close(h->db);
    free(h);
}

static int sqlite_insert(BB_ModelHandle *handle, BB_Schema *schema, void *entity)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    char sql[1024] = {0};
    strcat(sql, "INSERT INTO ");
    strcat(sql, schema->name);
    strcat(sql, " (");

    // column names
    for (size_t i = 0; i < schema->field_count; i++)
    {
        strcat(sql, schema->fields[i].name);
        if (i < schema->field_count - 1)
            strcat(sql, ", ");
    }

    strcat(sql, ") VALUES (");

    for (size_t i = 0; i < schema->field_count; i++)
    {
        strcat(sql, "?");
        if (i < schema->field_count - 1)
            strcat(sql, ", ");
    }

    strcat(sql, ");");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    // bind values
    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];
        void *field_ptr = (char *)entity + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
                sqlite3_bind_int(stmt, i + 1, *(int *)field_ptr);
                break;

            case BB_FIELD_STRING:
                sqlite3_bind_text(stmt, i + 1,
                                  (char *)field_ptr,
                                  -1, SQLITE_STATIC);
                break;

            case BB_FIELD_BLOB:
                sqlite3_bind_blob(stmt, i + 1,
                                  field_ptr,
                                  f->size,
                                  SQLITE_STATIC);
                break;
        }
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

static int sqlite_find_by_id(BB_ModelHandle *handle, BB_Schema *schema, void *out, int id)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    BB_Field *pk = &schema->fields[schema->primary_key_index];

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT * FROM %s WHERE %s = ?;",
             schema->name, pk->name);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return -1;
    }

    for (size_t i = 0; i < schema->field_count; i++)
    {
        BB_Field *f = &schema->fields[i];
        void *field_ptr = (char *)out + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
                *(int *)field_ptr = sqlite3_column_int(stmt, i);
                break;

            case BB_FIELD_STRING:
            {
                const unsigned char *text = sqlite3_column_text(stmt, i);
                if (text)
                    strncpy((char *)field_ptr, (const char *)text, f->size);
                break;
            }

            case BB_FIELD_BLOB:
                memcpy(field_ptr,
                       sqlite3_column_blob(stmt, i),
                       f->size);
                break;
        }
    }

    sqlite3_finalize(stmt);
    return 0;
}

static int sqlite_update(BB_ModelHandle *handle,
                         BB_Schema *schema,
                         void *entity)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    BB_Field *pk = &schema->fields[schema->primary_key_index];

    char sql[1024] = {0};
    strcat(sql, "UPDATE ");
    strcat(sql, schema->name);
    strcat(sql, " SET ");

    // SET clause (skip PK)
    int first = 1;
    for (size_t i = 0; i < schema->field_count; i++)
    {
        if ((int)i == schema->primary_key_index)
            continue;

        if (!first)
            strcat(sql, ", ");

        strcat(sql, schema->fields[i].name);
        strcat(sql, " = ?");

        first = 0;
    }

    strcat(sql, " WHERE ");
    strcat(sql, pk->name);
    strcat(sql, " = ?;");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    // bind values (non-PK first)
    int bind_index = 1;

    for (size_t i = 0; i < schema->field_count; i++)
    {
        if ((int)i == schema->primary_key_index)
            continue;

        BB_Field *f = &schema->fields[i];
        void *field_ptr = (char *)entity + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
                sqlite3_bind_int(stmt, bind_index++, *(int *)field_ptr);
                break;

            case BB_FIELD_STRING:
                sqlite3_bind_text(stmt, bind_index++,
                                  (char *)field_ptr,
                                  -1, SQLITE_TRANSIENT);
                break;

            case BB_FIELD_BLOB:
                sqlite3_bind_blob(stmt, bind_index++,
                                  field_ptr,
                                  f->size,
                                  SQLITE_TRANSIENT);
                break;
        }
    }

    // bind PK last
    void *pk_ptr = (char *)entity + pk->offset;
    sqlite3_bind_int(stmt, bind_index, *(int *)pk_ptr);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

static int sqlite_remove(BB_ModelHandle *handle,
                         BB_Schema *schema,
                         int id)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    BB_Field *pk = &schema->fields[schema->primary_key_index];

    char sql[512];
    snprintf(sql, sizeof(sql),
             "DELETE FROM %s WHERE %s = ?;",
             schema->name, pk->name);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

static BB_ModelAPI model_sqlite_api = {
    .name       = "sqlite",
    .open       = sqlite_open,
    .close      = sqlite_close,
    .insert     = sqlite_insert,
    .find_by_id = sqlite_find_by_id,
    .update     = sqlite_update,
    .remove     = sqlite_remove
};

const BB_ModelAPI *bb_model_sqlite_api(void)
{
    return &model_sqlite_api;
}