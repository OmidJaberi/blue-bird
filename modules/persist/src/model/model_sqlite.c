#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "blue-bird/persist/model/model_sqlite.h"

/* ---------------------------
 * Connection pool
 *
 * A single sqlite3* handed out to every caller serializes all model
 * access onto one connection: concurrent threads doing insert/find/
 * query on the same handle end up fighting over it, and nothing stops
 * two threads from driving the same sqlite3_stmt at once. Instead,
 * BB_ModelSQLiteHandle owns a small pool of independent connections
 * opened against the same database file; each call borrows one for
 * the duration of a single statement and returns it afterward, so
 * concurrent callers get concurrent connections instead of contending
 * on one.
 *
 * Connections are opened lazily, up to a fixed capacity, and reused
 * from a free list. If every connection is checked out, a caller
 * blocks on a condition variable until one is released rather than
 * over-opening connections without bound.
 *
 * ":memory:" (and other private in-memory URIs) is a special case:
 * each sqlite3_open() against ":memory:" creates its own independent,
 * empty database, so pooling it normally would make every other
 * borrowed connection see a blank database. For those URIs the pool
 * collapses to a single shared connection, which reproduces the old
 * (correct, if serialized) single-connection behavior.
 * --------------------------- */

#define BB_SQLITE_POOL_MAX_CONNS 8
#define BB_SQLITE_BUSY_TIMEOUT_MS 5000

typedef struct {
    sqlite3 *db;
    int in_use;
} bb_sqlite_conn_t;

typedef struct {
    char *uri;
    int is_private_memory;

    pthread_mutex_t lock;
    pthread_cond_t released;

    bb_sqlite_conn_t conns[BB_SQLITE_POOL_MAX_CONNS];
    size_t capacity;   /* usable slots in conns[]; 1 for in-memory URIs */
    size_t opened;     /* how many of those slots hold a live connection */
} BB_ModelSQLiteHandle;

static int uri_is_private_memory(const char *uri)
{
    if (!uri || uri[0] == '\0')
        return 1; /* sqlite3_open("") makes a private temporary DB */

    if (strcmp(uri, ":memory:") == 0)
        return 1;

    /* file::memory: without cache=shared is still a private, per-
     * connection database -- a pool would fragment it just like
     * ":memory:" does. We don't try to detect cache=shared here;
     * shared-cache in-memory URIs are rare enough in this codebase
     * that collapsing to one connection is the safe default. */
    if (strncmp(uri, "file::memory:", strlen("file::memory:")) == 0)
        return 1;

    return 0;
}

static int configure_connection(sqlite3 *db, int is_private_memory)
{
    sqlite3_busy_timeout(db, BB_SQLITE_BUSY_TIMEOUT_MS);

    if (!is_private_memory)
    {
        /* Best-effort: WAL lets readers and a writer proceed
         * concurrently instead of blocking each other on every
         * statement. Some filesystems (network mounts, etc.) can't
         * support it -- ignore failure and fall back to the default
         * journal mode rather than failing the whole connection. */
        sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    }

    return 0;
}

static int open_one_connection(const char *uri, int is_private_memory, sqlite3 **out_db)
{
    sqlite3 *db = NULL;
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;

    /* SQLITE_OPEN_NOMUTEX is safe here: the pool never hands the same
     * connection to two threads at once, so sqlite3 doesn't need to
     * serialize access to it internally. */
    if (sqlite3_open_v2(uri, &db, flags, NULL) != SQLITE_OK)
    {
        if (db)
            sqlite3_close(db);
        return -1;
    }

    configure_connection(db, is_private_memory);

    *out_db = db;
    return 0;
}

/* Borrow a connection from the pool, opening a new one (up to
 * capacity) if every existing connection is checked out, and blocking
 * until one is released once capacity is reached. Returns NULL only
 * on an unrecoverable open failure. */
static sqlite3 *pool_acquire(BB_ModelSQLiteHandle *h)
{
    pthread_mutex_lock(&h->lock);

    for (;;)
    {
        for (size_t i = 0; i < h->opened; i++)
        {
            if (!h->conns[i].in_use)
            {
                h->conns[i].in_use = 1;
                sqlite3 *db = h->conns[i].db;
                pthread_mutex_unlock(&h->lock);
                return db;
            }
        }

        if (h->opened < h->capacity)
        {
            sqlite3 *db;
            if (open_one_connection(h->uri, h->is_private_memory, &db) != 0)
            {
                pthread_mutex_unlock(&h->lock);
                return NULL;
            }

            size_t idx = h->opened++;
            h->conns[idx].db = db;
            h->conns[idx].in_use = 1;

            pthread_mutex_unlock(&h->lock);
            return db;
        }

        /* Every slot is open and checked out -- wait for one back. */
        pthread_cond_wait(&h->released, &h->lock);
    }
}

static void pool_release(BB_ModelSQLiteHandle *h, sqlite3 *db)
{
    if (!db)
        return;

    pthread_mutex_lock(&h->lock);

    for (size_t i = 0; i < h->opened; i++)
    {
        if (h->conns[i].db == db)
        {
            h->conns[i].in_use = 0;
            break;
        }
    }

    pthread_cond_signal(&h->released);
    pthread_mutex_unlock(&h->lock);
}

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

static const char *query_op_to_sql(bb_query_op_t op)
{
    switch (op)
    {
        case BB_OP_EQ:   return "=";
        case BB_OP_NE:   return "!=";
        case BB_OP_LT:   return "<";
        case BB_OP_LTE:  return "<=";
        case BB_OP_GT:   return ">";
        case BB_OP_GTE:  return ">=";
        case BB_OP_LIKE: return "LIKE";
        default:         return "=";
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
    BB_ModelSQLiteHandle *h = calloc(1, sizeof(*h));
    if (!h) return NULL;

    h->uri = uri ? strdup(uri) : strdup("");
    if (!h->uri)
    {
        free(h);
        return NULL;
    }

    h->is_private_memory = uri_is_private_memory(uri);
    h->capacity = h->is_private_memory ? 1 : BB_SQLITE_POOL_MAX_CONNS;
    h->opened = 0;

    if (pthread_mutex_init(&h->lock, NULL) != 0)
    {
        free(h->uri);
        free(h);
        return NULL;
    }

    if (pthread_cond_init(&h->released, NULL) != 0)
    {
        pthread_mutex_destroy(&h->lock);
        free(h->uri);
        free(h);
        return NULL;
    }

    /* Open (and validate) the first connection eagerly so a bad path
     * or unwritable file is reported to the caller from open(), the
     * same as the old single-connection behavior. */
    sqlite3 *db;
    if (open_one_connection(h->uri, h->is_private_memory, &db) != 0)
    {
        pthread_cond_destroy(&h->released);
        pthread_mutex_destroy(&h->lock);
        free(h->uri);
        free(h);
        return NULL;
    }

    h->conns[0].db = db;
    h->conns[0].in_use = 0;
    h->opened = 1;

    return (bb_model_handle_t *)h;
}

static void sqlite_close(bb_model_handle_t *handle)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    if (!h) return;

    /* Not locked: by the time close() is called the caller must have
     * stopped using this handle from every other thread, same
     * contract as before pooling. */
    for (size_t i = 0; i < h->opened; i++)
        sqlite3_close(h->conns[i].db);

    pthread_cond_destroy(&h->released);
    pthread_mutex_destroy(&h->lock);
    free(h->uri);
    free(h);
}

static int sqlite_insert(bb_model_handle_t *handle, bb_schema_t *schema, void *entity)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    sqlite3 *db = pool_acquire(h);
    if (!db) return -1;

    if (ensure_table(db, schema) != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
    {
        pool_release(h, db);
        return -1;
    }

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
        pool_release(h, db);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

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
    pool_release(h, db);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

static int sqlite_find_by_pk(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const void *key)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    sqlite3 *db = pool_acquire(h);
    if (!db) return -1;

    if (ensure_table(db, schema) != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
    {
        pool_release(h, db);
        return -1;
    }

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT * FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= sql_buf_append(&sql, pk->name);
    err |= sql_buf_append(&sql, " = ?;");

    if (err)
    {
        sql_buf_free(&sql);
        pool_release(h, db);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

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
        pool_release(h, db);
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
    pool_release(h, db);
    return 0;
}

static int sqlite_update(bb_model_handle_t *handle,
                         bb_schema_t *schema,
                         void *entity)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    sqlite3 *db = pool_acquire(h);
    if (!db) return -1;

    if (ensure_table(db, schema) != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
    {
        pool_release(h, db);
        return -1;
    }

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
        pool_release(h, db);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

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
    {
        pool_release(h, db);
        return -1;
    }

    // Check how many rows were actually changed
    int changes = sqlite3_changes(db);
    pool_release(h, db);
    return (changes > 0) ? 0 : -1;
}

static int sqlite_remove(bb_model_handle_t *handle, bb_schema_t *schema, const void *key)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    sqlite3 *db = pool_acquire(h);
    if (!db) return -1;

    if (ensure_table(db, schema) != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
    {
        pool_release(h, db);
        return -1;
    }

    int err = 0;
    err |= sql_buf_append(&sql, "DELETE FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= sql_buf_append(&sql, pk->name);
    err |= sql_buf_append(&sql, " = ?;");

    if (err)
    {
        sql_buf_free(&sql);
        pool_release(h, db);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

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
    {
        pool_release(h, db);
        return -1;
    }

    // Check how many rows were actually changed
    int changes = sqlite3_changes(db);
    pool_release(h, db);
    return (changes > 0) ? 0 : -1;
}

static int sqlite_find_all(bb_model_handle_t *handle,
                           bb_schema_t *schema,
                           void **out_array,
                           size_t *out_count)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    sqlite3 *db = pool_acquire(h);
    if (!db) return -1;

    if (ensure_table(db, schema) != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
    {
        pool_release(h, db);
        return -1;
    }

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT * FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, ";");

    if (err)
    {
        sql_buf_free(&sql);
        pool_release(h, db);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    size_t capacity = 8;
    size_t count = 0;

    void *buffer = malloc(schema->struct_size * capacity);
    if (!buffer)
    {
        sqlite3_finalize(stmt);
        pool_release(h, db);
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
                pool_release(h, db);
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
    pool_release(h, db);

    *out_array = buffer;
    *out_count = count;

    return 0;
}

static int sqlite_find_first_by_field(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const char *field_name, const void *value)
{
    BB_ModelSQLiteHandle *h =
        (BB_ModelSQLiteHandle *)handle;
    sqlite3 *db = pool_acquire(h);
    if (!db) return -1;

    bb_field_t *field =
        bb_schema_find_field(schema, field_name);

    if (!field)
    {
        pool_release(h, db);
        return -1;
    }

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
    {
        pool_release(h, db);
        return -1;
    }

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT * FROM ");
    err |= sql_buf_append(&sql, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= sql_buf_append(&sql, field->name);
    err |= sql_buf_append(&sql, " = ? LIMIT 1;");

    if (err)
    {
        sql_buf_free(&sql);
        pool_release(h, db);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        pool_release(h, db);
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
            pool_release(h, db);
            return -1;
    }

    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        pool_release(h, db);
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
    pool_release(h, db);

    return 0;
}

static int sqlite_query(bb_model_handle_t *handle, bb_schema_t *schema, const bb_query_t *q, void **out_array, size_t *out_count)
{
    BB_ModelSQLiteHandle *h = (BB_ModelSQLiteHandle *)handle;
    sqlite3 *db = pool_acquire(h);
    if (!db) return -1;

    if (ensure_table(db, schema) != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
    {
        pool_release(h, db);
        return -1;
    }

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT * FROM ");
    err |= sql_buf_append(&sql, schema->name);

    if (q->condition_count > 0)
    {
        err |= sql_buf_append(&sql, " WHERE ");

        for (size_t i = 0; i < q->condition_count; i++)
        {
            if (i > 0)
                err |= sql_buf_append(&sql, " AND ");

            err |= sql_buf_append(&sql, q->conditions[i].field);
            err |= sql_buf_append(&sql, " ");
            err |= sql_buf_append(&sql, query_op_to_sql(q->conditions[i].op));
            err |= sql_buf_append(&sql, " ?");
        }
    }

    if (q->sort_count > 0)
    {
        err |= sql_buf_append(&sql, " ORDER BY ");

        for (size_t i = 0; i < q->sort_count; i++)
        {
            if (i > 0)
                err |= sql_buf_append(&sql, ", ");

            err |= sql_buf_append(&sql, q->sorts[i].field);
            err |= sql_buf_append(&sql,
                q->sorts[i].direction == BB_ORDER_ASC ? " ASC" : " DESC");
        }
    }

    /* SQLite requires LIMIT to precede OFFSET; an offset with no limit
     * needs an explicit "no limit" sentinel. */
    if (q->limit >= 0)
        err |= sql_buf_append(&sql, " LIMIT ?");
    else if (q->offset >= 0)
        err |= sql_buf_append(&sql, " LIMIT -1");

    if (q->offset >= 0)
        err |= sql_buf_append(&sql, " OFFSET ?");

    err |= sql_buf_append(&sql, ";");

    if (err)
    {
        sql_buf_free(&sql);
        pool_release(h, db);
        return -1;
    }

    sqlite3_stmt *stmt;
    int prepare_rc = sqlite3_prepare_v2(db, sql.data, -1, &stmt, NULL);
    sql_buf_free(&sql);

    if (prepare_rc != SQLITE_OK)
    {
        pool_release(h, db);
        return -1;
    }

    int bind_index = 1;

    for (size_t i = 0; i < q->condition_count; i++)
    {
        const bb_query_cond_t *cond = &q->conditions[i];
        bb_field_t *f = bb_schema_find_field(schema, cond->field);

        if (!f)
        {
            sqlite3_finalize(stmt);
            pool_release(h, db);
            return -1;
        }

        switch (f->type)
        {
            case BB_FIELD_INT:
                sqlite3_bind_int(stmt, bind_index++, *(const int *)cond->value);
                break;

            case BB_FIELD_STRING:
            case BB_FIELD_UUID:
                sqlite3_bind_text(stmt, bind_index++,
                                  (const char *)cond->value,
                                  -1, SQLITE_STATIC);
                break;

            case BB_FIELD_BLOB:
                sqlite3_bind_blob(stmt, bind_index++,
                                  cond->value,
                                  (int)f->size,
                                  SQLITE_STATIC);
                break;
        }
    }

    if (q->limit >= 0)
        sqlite3_bind_int(stmt, bind_index++, (int)q->limit);

    if (q->offset >= 0)
        sqlite3_bind_int(stmt, bind_index++, (int)q->offset);

    size_t capacity = 8;
    size_t count = 0;

    void *buffer = malloc(schema->struct_size * capacity);
    if (!buffer)
    {
        sqlite3_finalize(stmt);
        pool_release(h, db);
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
                pool_release(h, db);
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
    pool_release(h, db);

    if (count == 0)
    {
        free(buffer);
        buffer = NULL;
    }

    *out_array = buffer;
    *out_count = count;

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
    .find_first_by_field = sqlite_find_first_by_field,
    .query               = sqlite_query
};

const bb_model_api_t *bb_model_sqlite_api(void)
{
    return &model_sqlite_api;
}
