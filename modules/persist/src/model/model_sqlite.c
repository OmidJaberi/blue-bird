#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "blue-bird/persist/model/model_sqlite.h"

typedef struct {
    sqlite3 *db;
} BB_ModelSQLiteHandle;

static const char *field_type_to_sql(bb_field_type_t type)
{
    switch (type)
    {
        case BB_FIELD_INT: return "INTEGER";
        case BB_FIELD_STRING: return "TEXT";
        case BB_FIELD_UUID: return "TEXT";
        case BB_FIELD_BLOB: return "BLOB";
        default: return "BLOB";
    }
}

/* ---------------------------
 * Dynamic SQL buffer
 *
 * Table and column names come from schema metadata whose length is not
 * bounded by anything in this module. Building SQL strings into fixed
 * stack buffers with strcat() (unchecked -> stack buffer overflow) or
 * snprintf() (silently truncates -> queries the wrong table/column) is
 * unsafe once a schema has a long name or enough fields. This buffer
 * grows on demand instead, so there is no fixed limit to overrun.
 * --------------------------- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sql_buf_t;

static int sql_buf_init(sql_buf_t *b)
{
    b->cap = 128;
    b->len = 0;
    b->data = malloc(b->cap);

    if (!b->data)
        return -1;

    b->data[0] = '\0';
    return 0;
}

static int sql_buf_append(sql_buf_t *b, const char *s)
{
    size_t slen = strlen(s);
    size_t needed = b->len + slen + 1; /* +1 for NUL */

    if (needed > b->cap)
    {
        size_t new_cap = b->cap;

        while (new_cap < needed)
            new_cap *= 2;

        char *tmp = realloc(b->data, new_cap);
        if (!tmp)
            return -1;

        b->data = tmp;
        b->cap = new_cap;
    }

    memcpy(b->data + b->len, s, slen + 1); /* copies the NUL too */
    b->len += slen;
    return 0;
}

static void sql_buf_free(sql_buf_t *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static int ensure_table(sqlite3 *db, bb_schema_t *schema)
{
    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return SQLITE_NOMEM;

    int err = 0;
    err |= sql_buf_append(&sql, "CREATE TABLE IF NOT EXISTS ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " (");

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];

        err |= sql_buf_append(&sql, f->name);
        err |= sql_buf_append(&sql, " ");
        err |= sql_buf_append(&sql, field_type_to_sql(f->type));

        if (i == schema->primary_key_index)
            err |= sql_buf_append(&sql, " PRIMARY KEY");

        if (i < schema->field_count - 1)
            err |= sql_buf_append(&sql, ", ");
    }

    err |= sql_buf_append(&sql, ");");

    if (err)
    {
        sql_buf_free(&sql);
        return SQLITE_NOMEM;
    }

    int rc = sqlite3_exec(db, sql.data, NULL, NULL, NULL);
    sql_buf_free(&sql);
    return rc;
}

static bb_model_handle_t *sqlite_open(const char *uri)
{
    BB_ModelSQLiteHandle *h = malloc(sizeof(*h));
    if (!h) return NULL;

    if (sqlite3_open(uri, &h->db) != SQLITE_OK)
    {
        free(h);
        return NULL;
    }

    return (bb_model_handle_t *)h;
}

static void sqlite_close(bb_model_handle_t *handle)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    if (!h) return;

    sqlite3_close(h->db);
    free(h);
}

static int sqlite_insert(bb_model_handle_t *handle, bb_schema_t *schema, void *entity)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "INSERT INTO ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " (");

    // column names
    for (size_t i = 0; i < schema->field_count; i++)
    {
        err |= sql_buf_append(&sql, schema->fields[i].name);
        if (i < schema->field_count - 1)
            err |= sql_buf_append(&sql, ", ");
    }

    err |= sql_buf_append(&sql, ") VALUES (");

    for (size_t i = 0; i < schema->field_count; i++)
    {
        err |= sql_buf_append(&sql, "?");
        if (i < schema->field_count - 1)
            err |= sql_buf_append(&sql, ", ");
    }

    err |= sql_buf_append(&sql, ");");

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(h->db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
        return -1;

    // bind values
    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];
        void *field_ptr = (char *)entity + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
                sqlite3_bind_int(stmt, (int)i + 1, *(int *)field_ptr);
                break;

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
                sqlite3_bind_text(stmt, (int)i + 1,
                                  (char *)field_ptr,
                                  -1, SQLITE_STATIC);
                break;

            case BB_FIELD_BLOB:
                sqlite3_bind_blob(stmt, (int)i + 1,
                                  field_ptr,
                                  (int)f->size,
                                  SQLITE_STATIC);
                break;
        }
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

static int sqlite_find_by_pk(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const void *key)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT * FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= sql_buf_append(&sql, pk->name);
    err |= sql_buf_append(&sql, " = ?;");

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(h->db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
        return -1;

    switch (pk->type)
    {
        case BB_FIELD_INT:
            sqlite3_bind_int(stmt, 1, *(int *)key);
            break;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
            sqlite3_bind_text(stmt, 1, (const char *)key, -1, SQLITE_STATIC);
            break;

        case BB_FIELD_BLOB:
            sqlite3_bind_blob(stmt, 1, key, (int)pk->size, SQLITE_STATIC);
            break;
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return -1;
    }

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];
        void *field_ptr = (char *)out + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
                *(int *)field_ptr = sqlite3_column_int(stmt, (int)i);
                break;

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
            {
                const unsigned char *text = sqlite3_column_text(stmt, (int)i);
                if (text)
                    strncpy((char *)field_ptr, (const char *)text, f->size);
                break;
            }

            case BB_FIELD_BLOB:
                memcpy(field_ptr,
                       sqlite3_column_blob(stmt, (int)i),
                       f->size);
                break;
        }
    }

    sqlite3_finalize(stmt);
    return 0;
}

static int sqlite_update(bb_model_handle_t *handle,
                         bb_schema_t *schema,
                         void *entity)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "UPDATE ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " SET ");

    // SET clause (skip PK)
    int first = 1;
    for (size_t i = 0; i < schema->field_count; i++)
    {
        if (i == schema->primary_key_index)
            continue;

        if (!first)
            err |= sql_buf_append(&sql, ", ");

        err |= sql_buf_append(&sql, schema->fields[i].name);
        err |= sql_buf_append(&sql, " = ?");

        first = 0;
    }

    err |= sql_buf_append(&sql, " WHERE ");
    err |= sql_buf_append(&sql, pk->name);
    err |= sql_buf_append(&sql, " = ?;");

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(h->db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
        return -1;

    // bind values (non-PK first)
    int bind_index = 1;

    for (size_t i = 0; i < schema->field_count; i++)
    {
        if (i == schema->primary_key_index)
            continue;

        bb_field_t *f = &schema->fields[i];
        void *field_ptr = (char *)entity + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
                sqlite3_bind_int(stmt, bind_index++, *(int *)field_ptr);
                break;

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
                sqlite3_bind_text(stmt, bind_index++,
                                  (char *)field_ptr,
                                  -1, SQLITE_TRANSIENT);
                break;

            case BB_FIELD_BLOB:
                sqlite3_bind_blob(stmt, bind_index++,
                                  field_ptr,
                                  (int)f->size,
                                  SQLITE_TRANSIENT);
                break;
        }
    }

    // bind PK last
    void *pk_ptr = (char *)entity + pk->offset;

    switch (pk->type)
    {
        case BB_FIELD_INT:
            sqlite3_bind_int(stmt, bind_index, *(int *)pk_ptr);
            break;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
            sqlite3_bind_text(stmt, bind_index, (char *)pk_ptr, -1, SQLITE_TRANSIENT);
            break;

        case BB_FIELD_BLOB:
            sqlite3_bind_blob(stmt, bind_index, pk_ptr, (int)pk->size, SQLITE_TRANSIENT);
            break;
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return -1;

    // Check how many rows were actually changed
    int changes = sqlite3_changes(h->db);
    return (changes > 0) ? 0 : -1;
}

static int sqlite_remove(bb_model_handle_t *handle, bb_schema_t *schema, const void *key)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "DELETE FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= sql_buf_append(&sql, pk->name);
    err |= sql_buf_append(&sql, " = ?;");

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(h->db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
        return -1;

    switch (pk->type)
    {
        case BB_FIELD_INT:
            sqlite3_bind_int(stmt, 1, *(int *)key);
            break;

        case BB_FIELD_UUID:
        case BB_FIELD_STRING:
            sqlite3_bind_text(stmt, 1, (const char *)key, -1, SQLITE_STATIC);
            break;

        case BB_FIELD_BLOB:
            sqlite3_bind_blob(stmt, 1, key, (int)pk->size, SQLITE_STATIC);
            break;
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return -1;

    // Check how many rows were actually changed
    int changes = sqlite3_changes(h->db);
    return (changes > 0) ? 0 : -1;
}

static int sqlite_find_all(bb_model_handle_t *handle,
                           bb_schema_t *schema,
                           void **out_array,
                           size_t *out_count)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;

    if (ensure_table(h->db, schema) != SQLITE_OK)
        return -1;

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT * FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, ";");

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(h->db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
        return -1;

    size_t capacity = 8;
    size_t count = 0;

    void *buffer = malloc(schema->struct_size * capacity);
    if (!buffer)
    {
        sqlite3_finalize(stmt);
        return -1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= capacity)
        {
            capacity *= 2;
            void *tmp = realloc(buffer, schema->struct_size * capacity);
            if (!tmp)
            {
                free(buffer);
                sqlite3_finalize(stmt);
                return -1;
            }
            buffer = tmp;
        }

        void *entity = (char *)buffer + (count * schema->struct_size);

        for (size_t i = 0; i < schema->field_count; i++)
        {
            bb_field_t *f = &schema->fields[i];
            void *field_ptr = (char *)entity + f->offset;

            switch (f->type)
            {
                case BB_FIELD_INT:
                    *(int *)field_ptr = sqlite3_column_int(stmt, (int)i);
                    break;

                case BB_FIELD_STRING:
                case BB_FIELD_UUID:
                {
                    const unsigned char *text = sqlite3_column_text(stmt, (int)i);
                    if (text)
                        strncpy((char *)field_ptr, (const char *)text, f->size);
                    break;
                }

                case BB_FIELD_BLOB:
                    memcpy(field_ptr,
                           sqlite3_column_blob(stmt, (int)i),
                           f->size);
                    break;
            }
        }

        count++;
    }

    sqlite3_finalize(stmt);

    *out_array = buffer;
    *out_count = count;

    return 0;
}

static int sqlite_find_first_by_field(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const char *field_name, const void *value)
{
    BB_ModelSQLiteHandle *h =
        (BB_ModelSQLiteHandle *)handle;

    bb_field_t *field =
        bb_schema_find_field(schema, field_name);

    if (!field)
        return -1;

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT * FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= sql_buf_append(&sql, field->name);
    err |= sql_buf_append(&sql, " = ? LIMIT 1;");

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(h->db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        return -1;
    }

    switch (field->type)
    {
        case BB_FIELD_INT:
            sqlite3_bind_int(stmt,
                             1,
                             *(int *)value);
            break;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
            sqlite3_bind_text(stmt,
                              1,
                              (const char *)value,
                              -1,
                              SQLITE_STATIC);
            break;

        default:
            sqlite3_finalize(stmt);
            return -1;
    }

    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return -1;
    }

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];

        void *field_ptr =
            (char *)out + f->offset;

        switch (f->type)
        {
            case BB_FIELD_INT:
                *(int *)field_ptr =
                    sqlite3_column_int(stmt, (int)i);
                break;

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
            {
                const unsigned char *text =
                    sqlite3_column_text(stmt, (int)i);

                if (text)
                {
                    strncpy((char *)field_ptr,
                            (const char *)text,
                            f->size);
                }

                break;
            }

            default:
                break;
        }
    }

    sqlite3_finalize(stmt);

    return 0;
}

static bb_model_api_t model_sqlite_api = {
    .name                = "sqlite",
    .open                = sqlite_open,
    .close               = sqlite_close,
    .insert              = sqlite_insert,
    .find_by_pk          = sqlite_find_by_pk,
    .update              = sqlite_update,
    .remove              = sqlite_remove,
    .find_all            = sqlite_find_all,
    .find_first_by_field = sqlite_find_first_by_field
};

const bb_model_api_t *bb_model_sqlite_api(void)
{
    return &model_sqlite_api;
}
