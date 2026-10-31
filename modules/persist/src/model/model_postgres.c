#include <libpq-fe.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blue-bird/persist/model/model_postgres.h"

#include <blue-bird/utils/platform.h>

/* ---------------------------
 * PostgreSQL model backend.
 *
 * Mirrors the SQLite backend's behaviour on purpose (same return values:
 * 0 on success, -1 on error / not found / no row affected; tables are created
 * on first use; AUTO_GENERATE/REQUIRED/UNIQUE flags are not interpreted yet).
 * Differences that are intentional:
 *   - identifiers are quoted with PQescapeIdentifier,
 *   - values always travel as bound parameters (PQexecParams),
 *   - BB_FIELD_UUID maps to a native UUID column,
 *   - strings read back are always NUL-terminated and result structs are
 *     zero-initialised before being filled.
 *
 * Connection pool: the handle owns up to BB_PG_POOL_MAX_CONNS independent
 * PGconns, opened lazily. Every call borrows one for its duration and gives it
 * back, so concurrent callers run concurrently (a PGconn itself must never be
 * used by two threads at once, and the pool guarantees it isn't). When all are
 * checked out a caller waits until one is returned.
 *
 * Tables are created with CREATE TABLE IF NOT EXISTS the first time a schema
 * is used on a handle and remembered afterwards (SQLite re-checks on every
 * call). Creation is serialised: concurrent CREATE TABLE IF NOT EXISTS on one
 * name can fail on the server with a duplicate-key error.
 * --------------------------- */

#define BB_PG_POOL_MAX_CONNS 8

typedef struct {
    PGconn *conn;   /* NULL until the slot has been connected */
    int in_use;
} pg_slot_t;

typedef struct {
    char *uri;

    pthread_mutex_t lock;       /* guards slots[] bookkeeping */
    pthread_cond_t returned;    /* signalled when a slot is released */
    pg_slot_t slots[BB_PG_POOL_MAX_CONNS];

    pthread_mutex_t tables_lock; /* guards tables[] and serialises DDL */
    char **tables;               /* names already created on this handle */
    size_t tables_count;
    size_t tables_cap;
} BB_ModelPostgresHandle;

/* ---------------------------
 * Small growable SQL buffer
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
    size_t needed = b->len + slen + 1;

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

    memcpy(b->data + b->len, s, slen + 1);
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

static int append_ident(sql_buf_t *b, PGconn *conn, const char *name)
{
    char *quoted = PQescapeIdentifier(conn, name, strlen(name));
    if (!quoted)
        return -1;

    int rc = sql_buf_append(b, quoted);
    PQfreemem(quoted);
    return rc;
}

static int append_param(sql_buf_t *b, size_t index)
{
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "$%zu", index);
    return sql_buf_append(b, tmp);
}

/* ---------------------------
 * Schema helpers
 * --------------------------- */

static const char *field_type_to_sql(bb_field_type_t type)
{
    switch (type)
    {
        case BB_FIELD_INT:    return "INTEGER"; /* entities hold a C int */
        case BB_FIELD_STRING: return "TEXT";
        case BB_FIELD_UUID:   return "UUID";
        case BB_FIELD_BLOB:   return "BYTEA";
        default:              return "BYTEA";
    }
}

static int schema_is_usable(const bb_schema_t *schema)
{
    return schema && schema->name && schema->fields &&
           schema->field_count > 0 &&
           schema->primary_key_index < schema->field_count;
}

/* ---------------------------
 * Bound parameters
 * --------------------------- */

#define INT_TEXT_LEN 24

typedef struct {
    const char **values;
    int *lengths;
    int *formats;
    char (*int_text)[INT_TEXT_LEN];
} pg_params_t;

static int params_init(pg_params_t *p, size_t n)
{
    p->values   = calloc(n, sizeof(*p->values));
    p->lengths  = calloc(n, sizeof(*p->lengths));
    p->formats  = calloc(n, sizeof(*p->formats));
    p->int_text = calloc(n, sizeof(*p->int_text));

    if (!p->values || !p->lengths || !p->formats || !p->int_text)
    {
        free(p->values);
        free(p->lengths);
        free(p->formats);
        free(p->int_text);
        memset(p, 0, sizeof(*p));
        return -1;
    }
    return 0;
}

static void params_free(pg_params_t *p)
{
    free(p->values);
    free(p->lengths);
    free(p->formats);
    free(p->int_text);
    memset(p, 0, sizeof(*p));
}

/* src points at the value in the entity (or at the key for lookups). */
static void params_set(pg_params_t *p, size_t i, const bb_field_t *f, const void *src)
{
    switch (f->type)
    {
        case BB_FIELD_INT:
            snprintf(p->int_text[i], INT_TEXT_LEN, "%d", *(const int *)src);
            p->values[i]  = p->int_text[i];
            p->formats[i] = 0;
            break;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
            p->values[i]  = (const char *)src;
            p->formats[i] = 0;
            break;

        case BB_FIELD_BLOB:
        default:
            p->values[i]  = (const char *)src;
            p->lengths[i] = (int)f->size;
            p->formats[i] = 1; /* binary */
            break;
    }
}

/* ---------------------------
 * Connection pool
 * --------------------------- */

static PGconn *connect_one(const char *uri)
{
    PGconn *conn = PQconnectdb(uri);
    if (!conn)
        return NULL;

    if (PQstatus(conn) != CONNECTION_OK)
    {
        /* Message only; never print the conninfo, it may contain a password. */
        fprintf(stderr, "bb postgres: connection failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }
    return conn;
}

static void pool_release(BB_ModelPostgresHandle *h, pg_slot_t *slot)
{
    if (!slot)
        return;

    pthread_mutex_lock(&h->lock);
    slot->in_use = 0;
    pthread_cond_signal(&h->returned);
    pthread_mutex_unlock(&h->lock);
}

/* Borrow a ready connection: reuse an idle one, else open a new one (up to
 * the pool size), else wait for one to come back. Connecting and
 * reconnecting happen outside the pool lock so a slow server does not block
 * callers that could use an idle connection. NULL means the server could not
 * be reached. */
static pg_slot_t *pool_acquire(BB_ModelPostgresHandle *h)
{
    pg_slot_t *slot = NULL;
    int needs_connect = 0;

    pthread_mutex_lock(&h->lock);

    for (;;)
    {
        for (size_t i = 0; i < BB_PG_POOL_MAX_CONNS && !slot; i++)
            if (h->slots[i].conn && !h->slots[i].in_use)
                slot = &h->slots[i];

        if (!slot)
        {
            for (size_t i = 0; i < BB_PG_POOL_MAX_CONNS && !slot; i++)
                if (!h->slots[i].conn && !h->slots[i].in_use)
                {
                    slot = &h->slots[i];
                    needs_connect = 1;
                }
        }

        if (slot)
            break;

        pthread_cond_wait(&h->returned, &h->lock);
    }

    slot->in_use = 1;
    pthread_mutex_unlock(&h->lock);

    if (needs_connect)
    {
        PGconn *conn = connect_one(h->uri);
        if (!conn)
        {
            pool_release(h, slot);
            return NULL;
        }

        pthread_mutex_lock(&h->lock);
        slot->conn = conn;
        pthread_mutex_unlock(&h->lock);
        return slot;
    }

    /* Idle connection may have been dropped by the server meanwhile. */
    if (PQstatus(slot->conn) != CONNECTION_OK)
    {
        PQreset(slot->conn);
        if (PQstatus(slot->conn) != CONNECTION_OK)
        {
            pool_release(h, slot);
            return NULL;
        }
    }

    return slot;
}

/* ---------------------------
 * Table creation
 * --------------------------- */

static int create_table(PGconn *conn, bb_schema_t *schema)
{
    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "CREATE TABLE IF NOT EXISTS ");
    err |= append_ident(&sql, conn, schema->name);
    err |= sql_buf_append(&sql, " (");

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];

        err |= append_ident(&sql, conn, f->name);
        err |= sql_buf_append(&sql, " ");
        err |= sql_buf_append(&sql, field_type_to_sql(f->type));

        if (i == schema->primary_key_index)
            err |= sql_buf_append(&sql, " PRIMARY KEY");

        if (i < schema->field_count - 1)
            err |= sql_buf_append(&sql, ", ");
    }

    err |= sql_buf_append(&sql, ")");

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    PGresult *res = PQexec(conn, sql.data);
    sql_buf_free(&sql);

    int ok = res && PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return ok ? 0 : -1;
}

static int tables_contains(const BB_ModelPostgresHandle *h, const char *name)
{
    for (size_t i = 0; i < h->tables_count; i++)
        if (strcmp(h->tables[i], name) == 0)
            return 1;
    return 0;
}

static void tables_remember(BB_ModelPostgresHandle *h, const char *name)
{
    /* Best effort: if this fails the table simply is not cached and the
     * next call runs CREATE TABLE IF NOT EXISTS again. */
    if (h->tables_count == h->tables_cap)
    {
        size_t cap = h->tables_cap ? h->tables_cap * 2 : 8;
        char **tmp = realloc(h->tables, cap * sizeof(*tmp));
        if (!tmp)
            return;
        h->tables = tmp;
        h->tables_cap = cap;
    }

    char *copy = bb_strdup(name);
    if (copy)
        h->tables[h->tables_count++] = copy;
}

static int ensure_table(BB_ModelPostgresHandle *h, PGconn *conn, bb_schema_t *schema)
{
    pthread_mutex_lock(&h->tables_lock);

    int rc = 0;
    if (!tables_contains(h, schema->name))
    {
        rc = create_table(conn, schema);
        if (rc == 0)
            tables_remember(h, schema->name);
    }

    pthread_mutex_unlock(&h->tables_lock);
    return rc;
}

/* ---------------------------
 * open / close
 * --------------------------- */

static bb_model_handle_t *pg_open(const char *uri)
{
    if (!uri || uri[0] == '\0')
        return NULL;

    BB_ModelPostgresHandle *h = calloc(1, sizeof(*h));
    if (!h)
        return NULL;

    h->uri = bb_strdup(uri);
    if (!h->uri)
    {
        free(h);
        return NULL;
    }

    if (pthread_mutex_init(&h->lock, NULL) != 0)
    {
        free(h->uri);
        free(h);
        return NULL;
    }

    if (pthread_cond_init(&h->returned, NULL) != 0)
    {
        pthread_mutex_destroy(&h->lock);
        free(h->uri);
        free(h);
        return NULL;
    }

    if (pthread_mutex_init(&h->tables_lock, NULL) != 0)
    {
        pthread_cond_destroy(&h->returned);
        pthread_mutex_destroy(&h->lock);
        free(h->uri);
        free(h);
        return NULL;
    }

    /* Connect the first slot eagerly so a bad URI or unreachable server is
     * reported from open(), not from the first query. */
    h->slots[0].conn = connect_one(h->uri);
    if (!h->slots[0].conn)
    {
        pthread_mutex_destroy(&h->tables_lock);
        pthread_cond_destroy(&h->returned);
        pthread_mutex_destroy(&h->lock);
        free(h->uri);
        free(h);
        return NULL;
    }

    return (bb_model_handle_t *)h;
}

static void pg_close(bb_model_handle_t *handle)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    if (!h)
        return;

    /* Not locked: by the time close() is called the caller must have stopped
     * using this handle from every other thread (same contract as SQLite). */
    for (size_t i = 0; i < BB_PG_POOL_MAX_CONNS; i++)
        if (h->slots[i].conn)
            PQfinish(h->slots[i].conn);

    for (size_t i = 0; i < h->tables_count; i++)
        free(h->tables[i]);
    free(h->tables);

    pthread_mutex_destroy(&h->tables_lock);
    pthread_cond_destroy(&h->returned);
    pthread_mutex_destroy(&h->lock);
    free(h->uri);
    free(h);
}

/* ---------------------------
 * insert
 * --------------------------- */

static int pg_insert_conn(PGconn *conn, bb_schema_t *schema, void *entity)
{
    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "INSERT INTO ");
    err |= append_ident(&sql, conn, schema->name);
    err |= sql_buf_append(&sql, " (");

    for (size_t i = 0; i < schema->field_count; i++)
    {
        err |= append_ident(&sql, conn, schema->fields[i].name);
        if (i < schema->field_count - 1)
            err |= sql_buf_append(&sql, ", ");
    }

    err |= sql_buf_append(&sql, ") VALUES (");

    for (size_t i = 0; i < schema->field_count; i++)
    {
        err |= append_param(&sql, i + 1);
        if (i < schema->field_count - 1)
            err |= sql_buf_append(&sql, ", ");
    }

    err |= sql_buf_append(&sql, ")");

    pg_params_t params;
    if (err || params_init(&params, schema->field_count) != 0)
    {
        sql_buf_free(&sql);
        return -1;
    }

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];
        params_set(&params, i, f, (char *)entity + f->offset);
    }

    PGresult *res = PQexecParams(conn, sql.data, (int)schema->field_count,
                                 NULL, params.values, params.lengths,
                                 params.formats, 0);
    sql_buf_free(&sql);
    params_free(&params);

    int ok = res && PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return ok ? 0 : -1;
}

static int pg_insert(bb_model_handle_t *handle, bb_schema_t *schema, void *entity)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    if (!h || !entity || !schema_is_usable(schema))
        return -1;

    pg_slot_t *slot = pool_acquire(h);
    int rc = -1;

    if (slot && ensure_table(h, slot->conn, schema) == 0)
        rc = pg_insert_conn(slot->conn, schema, entity);

    pool_release(h, slot);
    return rc;
}

/* ---------------------------
 * find_by_pk
 * --------------------------- */

static void read_column(PGresult *res, int row, int col, const bb_field_t *f, void *dst)
{
    int is_null = PQgetisnull(res, row, col);
    const char *text = is_null ? "" : PQgetvalue(res, row, col);

    switch (f->type)
    {
        case BB_FIELD_INT:
            *(int *)dst = is_null ? 0 : (int)strtol(text, NULL, 10);
            break;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
        {
            if (f->size == 0)
                break;

            size_t n = strlen(text);
            if (n > f->size - 1)
                n = f->size - 1;

            memcpy(dst, text, n);
            memset((char *)dst + n, 0, f->size - n); /* NUL-terminates */
            break;
        }

        case BB_FIELD_BLOB:
        default:
        {
            memset(dst, 0, f->size);
            if (is_null)
                break;

            size_t len = 0;
            unsigned char *bytes = PQunescapeBytea((const unsigned char *)text, &len);
            if (!bytes)
                break;

            memcpy(dst, bytes, len < f->size ? len : f->size);
            PQfreemem(bytes);
            break;
        }
    }
}

static void read_row(PGresult *res, int row, const bb_schema_t *schema, void *dst)
{
    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];
        read_column(res, row, (int)i, f, (char *)dst + f->offset);
    }
}

/* Explicit column list so result column i always is schema field i. */
static int append_column_list(sql_buf_t *b, PGconn *conn, const bb_schema_t *schema)
{
    int err = 0;
    for (size_t i = 0; i < schema->field_count; i++)
    {
        err |= append_ident(b, conn, schema->fields[i].name);
        if (i < schema->field_count - 1)
            err |= sql_buf_append(b, ", ");
    }
    return err;
}

static int affected_rows(PGresult *res)
{
    const char *n = PQcmdTuples(res);
    return n ? atoi(n) : 0;
}

static int pg_find_by_pk_conn(PGconn *conn, bb_schema_t *schema, void *out, const void *key)
{
    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT ");

    err |= append_column_list(&sql, conn, schema);
    err |= sql_buf_append(&sql, " FROM ");
    err |= append_ident(&sql, conn, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= append_ident(&sql, conn, pk->name);
    err |= sql_buf_append(&sql, " = $1");

    pg_params_t params;
    if (err || params_init(&params, 1) != 0)
    {
        sql_buf_free(&sql);
        return -1;
    }

    params_set(&params, 0, pk, key);

    PGresult *res = PQexecParams(conn, sql.data, 1, NULL, params.values,
                                 params.lengths, params.formats, 0);
    sql_buf_free(&sql);
    params_free(&params);

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) < 1)
    {
        PQclear(res);
        return -1;
    }

    read_row(res, 0, schema, out);

    PQclear(res);
    return 0;
}

static int pg_find_by_pk(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const void *key)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    if (!h || !out || !key || !schema_is_usable(schema))
        return -1;

    pg_slot_t *slot = pool_acquire(h);
    int rc = -1;

    if (slot && ensure_table(h, slot->conn, schema) == 0)
        rc = pg_find_by_pk_conn(slot->conn, schema, out, key);

    pool_release(h, slot);
    return rc;
}

/* ---------------------------
 * update
 * --------------------------- */

static int pg_update_conn(PGconn *conn, bb_schema_t *schema, void *entity)
{
    size_t n = schema->field_count;
    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "UPDATE ");
    err |= append_ident(&sql, conn, schema->name);
    err |= sql_buf_append(&sql, " SET ");

    /* SET clause: every field except the PK, as $1..$(n-1) */
    size_t param = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (i == schema->primary_key_index)
            continue;

        if (param > 0)
            err |= sql_buf_append(&sql, ", ");

        err |= append_ident(&sql, conn, schema->fields[i].name);
        err |= sql_buf_append(&sql, " = ");
        err |= append_param(&sql, ++param);
    }

    err |= sql_buf_append(&sql, " WHERE ");
    err |= append_ident(&sql, conn, pk->name);
    err |= sql_buf_append(&sql, " = ");
    err |= append_param(&sql, n); /* PK is the last parameter */

    pg_params_t params;
    if (err || params_init(&params, n) != 0)
    {
        sql_buf_free(&sql);
        return -1;
    }

    size_t slot = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (i == schema->primary_key_index)
            continue;

        bb_field_t *f = &schema->fields[i];
        params_set(&params, slot++, f, (char *)entity + f->offset);
    }
    params_set(&params, slot, pk, (char *)entity + pk->offset);

    PGresult *res = PQexecParams(conn, sql.data, (int)n, NULL, params.values,
                                 params.lengths, params.formats, 0);
    sql_buf_free(&sql);
    params_free(&params);

    int ok = res && PQresultStatus(res) == PGRES_COMMAND_OK && affected_rows(res) > 0;
    PQclear(res);
    return ok ? 0 : -1;
}

static int pg_update(bb_model_handle_t *handle, bb_schema_t *schema, void *entity)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    /* needs at least one non-PK field to set */
    if (!h || !entity || !schema_is_usable(schema) || schema->field_count < 2)
        return -1;

    pg_slot_t *slot = pool_acquire(h);
    int rc = -1;

    if (slot && ensure_table(h, slot->conn, schema) == 0)
        rc = pg_update_conn(slot->conn, schema, entity);

    pool_release(h, slot);
    return rc;
}

/* ---------------------------
 * remove
 * --------------------------- */

static int pg_remove_conn(PGconn *conn, bb_schema_t *schema, const void *key)
{
    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "DELETE FROM ");
    err |= append_ident(&sql, conn, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= append_ident(&sql, conn, pk->name);
    err |= sql_buf_append(&sql, " = $1");

    pg_params_t params;
    if (err || params_init(&params, 1) != 0)
    {
        sql_buf_free(&sql);
        return -1;
    }

    params_set(&params, 0, pk, key);

    PGresult *res = PQexecParams(conn, sql.data, 1, NULL, params.values,
                                 params.lengths, params.formats, 0);
    sql_buf_free(&sql);
    params_free(&params);

    int ok = res && PQresultStatus(res) == PGRES_COMMAND_OK && affected_rows(res) > 0;
    PQclear(res);
    return ok ? 0 : -1;
}

static int pg_remove(bb_model_handle_t *handle, bb_schema_t *schema, const void *key)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    if (!h || !key || !schema_is_usable(schema))
        return -1;

    pg_slot_t *slot = pool_acquire(h);
    int rc = -1;

    if (slot && ensure_table(h, slot->conn, schema) == 0)
        rc = pg_remove_conn(slot->conn, schema, key);

    pool_release(h, slot);
    return rc;
}

/* ---------------------------
 * find_all
 * --------------------------- */

static int pg_find_all_conn(PGconn *conn, bb_schema_t *schema,
                              void **out_array, size_t *out_count)
{
    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT ");
    err |= append_column_list(&sql, conn, schema);
    err |= sql_buf_append(&sql, " FROM ");
    err |= append_ident(&sql, conn, schema->name);

    if (err)
    {
        sql_buf_free(&sql);
        return -1;
    }

    PGresult *res = PQexec(conn, sql.data);
    sql_buf_free(&sql);

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        return -1;
    }

    size_t count = (size_t)PQntuples(res);

    /* Like SQLite: always hand back a non-NULL buffer (caller free()s it),
     * zero-initialised so unset bytes are never garbage. */
    void *buffer = calloc(count > 0 ? count : 1, schema->struct_size);
    if (!buffer)
    {
        PQclear(res);
        return -1;
    }

    for (size_t r = 0; r < count; r++)
        read_row(res, (int)r, schema, (char *)buffer + r * schema->struct_size);

    PQclear(res);

    *out_array = buffer;
    *out_count = count;
    return 0;
}

static int pg_find_all(bb_model_handle_t *handle, bb_schema_t *schema,
                       void **out_array, size_t *out_count)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    if (!h || !out_array || !out_count || !schema_is_usable(schema) ||
        schema->struct_size == 0)
        return -1;

    pg_slot_t *slot = pool_acquire(h);
    int rc = -1;

    if (slot && ensure_table(h, slot->conn, schema) == 0)
        rc = pg_find_all_conn(slot->conn, schema, out_array, out_count);

    pool_release(h, slot);
    return rc;
}

/* ---------------------------
 * find_first_by_field
 * --------------------------- */

static int pg_find_first_by_field_conn(PGconn *conn, bb_schema_t *schema, void *out,
                                         bb_field_t *field, const void *value)
{
    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT ");
    err |= append_column_list(&sql, conn, schema);
    err |= sql_buf_append(&sql, " FROM ");
    err |= append_ident(&sql, conn, schema->name);
    err |= sql_buf_append(&sql, " WHERE ");
    err |= append_ident(&sql, conn, field->name);
    err |= sql_buf_append(&sql, " = $1 LIMIT 1");

    pg_params_t params;
    if (err || params_init(&params, 1) != 0)
    {
        sql_buf_free(&sql);
        return -1;
    }

    params_set(&params, 0, field, value);

    PGresult *res = PQexecParams(conn, sql.data, 1, NULL, params.values,
                                 params.lengths, params.formats, 0);
    sql_buf_free(&sql);
    params_free(&params);

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) < 1)
    {
        PQclear(res);
        return -1;
    }

    read_row(res, 0, schema, out);

    PQclear(res);
    return 0;
}

static int pg_find_first_by_field(bb_model_handle_t *handle, bb_schema_t *schema, void *out,
                                  const char *field_name, const void *value)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    if (!h || !out || !field_name || !value || !schema_is_usable(schema))
        return -1;

    bb_field_t *field = bb_schema_find_field(schema, field_name);

    /* Same as SQLite: only INT/STRING/UUID fields can be searched. */
    if (!field || field->type == BB_FIELD_BLOB)
        return -1;

    pg_slot_t *slot = pool_acquire(h);
    int rc = slot ? pg_find_first_by_field_conn(slot->conn, schema, out, field, value) : -1;

    pool_release(h, slot);
    return rc;
}

static bb_model_api_t model_postgres_api = {
    .name                = "postgres",
    .open                = pg_open,
    .close               = pg_close,
    .insert              = pg_insert,
    .find_by_pk          = pg_find_by_pk,
    .update              = pg_update,
    .remove              = pg_remove,
    .find_all            = pg_find_all,
    .find_first_by_field = pg_find_first_by_field,
    .query               = NULL   /* bb_repo_find() falls back to find_all() */
};

const bb_model_api_t *bb_model_postgres_api(void)
{
    return &model_postgres_api;
}
