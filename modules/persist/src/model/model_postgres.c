#include <libpq-fe.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blue-bird/persist/model/model_postgres.h"

/* ---------------------------
 * Step 2: ensure_table, insert, find_by_pk.
 *
 * Mirrors the SQLite backend's behaviour on purpose (same return values,
 * same "create the table on first use", AUTO_GENERATE/REQUIRED/UNIQUE flags
 * are not interpreted yet). Differences that are intentional:
 *   - identifiers are quoted with PQescapeIdentifier,
 *   - values always travel as bound parameters (PQexecParams),
 *   - BB_FIELD_UUID maps to a native UUID column.
 *
 * update/remove/find_all/find_first_by_field are still placeholders that
 * return -1.
 *
 * One PGconn guarded by a mutex: calls are safe from several threads but
 * serialised. A connection pool comes later.
 * --------------------------- */

typedef struct {
    PGconn *conn;
    pthread_mutex_t lock;
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
 * Connection helpers (call with h->lock held)
 * --------------------------- */

static PGconn *conn_ready(BB_ModelPostgresHandle *h)
{
    if (PQstatus(h->conn) != CONNECTION_OK)
    {
        PQreset(h->conn);
        if (PQstatus(h->conn) != CONNECTION_OK)
            return NULL;
    }
    return h->conn;
}

static int ensure_table(PGconn *conn, bb_schema_t *schema)
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

    if (pthread_mutex_init(&h->lock, NULL) != 0)
    {
        free(h);
        return NULL;
    }

    h->conn = PQconnectdb(uri);
    if (!h->conn)
    {
        pthread_mutex_destroy(&h->lock);
        free(h);
        return NULL;
    }

    if (PQstatus(h->conn) != CONNECTION_OK)
    {
        /* Message only; never print the conninfo, it may contain a password. */
        fprintf(stderr, "bb postgres: connection failed: %s", PQerrorMessage(h->conn));
        PQfinish(h->conn);
        pthread_mutex_destroy(&h->lock);
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

    PQfinish(h->conn);
    pthread_mutex_destroy(&h->lock);
    free(h);
}

/* ---------------------------
 * insert
 * --------------------------- */

static int pg_insert_locked(PGconn *conn, bb_schema_t *schema, void *entity)
{
    if (ensure_table(conn, schema) != 0)
        return -1;

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

    pthread_mutex_lock(&h->lock);

    PGconn *conn = conn_ready(h);
    int rc = conn ? pg_insert_locked(conn, schema, entity) : -1;

    pthread_mutex_unlock(&h->lock);
    return rc;
}

/* ---------------------------
 * find_by_pk
 * --------------------------- */

static void read_column(PGresult *res, int col, const bb_field_t *f, void *dst)
{
    int is_null = PQgetisnull(res, 0, col);
    const char *text = is_null ? "" : PQgetvalue(res, 0, col);

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

static int pg_find_by_pk_locked(PGconn *conn, bb_schema_t *schema, void *out, const void *key)
{
    if (ensure_table(conn, schema) != 0)
        return -1;

    bb_field_t *pk = &schema->fields[schema->primary_key_index];

    sql_buf_t sql;
    if (sql_buf_init(&sql) != 0)
        return -1;

    int err = 0;
    err |= sql_buf_append(&sql, "SELECT ");

    /* Explicit column list so result column i always is schema field i. */
    for (size_t i = 0; i < schema->field_count; i++)
    {
        err |= append_ident(&sql, conn, schema->fields[i].name);
        if (i < schema->field_count - 1)
            err |= sql_buf_append(&sql, ", ");
    }

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

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];
        read_column(res, (int)i, f, (char *)out + f->offset);
    }

    PQclear(res);
    return 0;
}

static int pg_find_by_pk(bb_model_handle_t *handle, bb_schema_t *schema, void *out, const void *key)
{
    BB_ModelPostgresHandle *h = (BB_ModelPostgresHandle *)handle;
    if (!h || !out || !key || !schema_is_usable(schema))
        return -1;

    pthread_mutex_lock(&h->lock);

    PGconn *conn = conn_ready(h);
    int rc = conn ? pg_find_by_pk_locked(conn, schema, out, key) : -1;

    pthread_mutex_unlock(&h->lock);
    return rc;
}

/* ---------------------------
 * Not implemented yet
 * --------------------------- */

static int pg_update(bb_model_handle_t *h, bb_schema_t *s, void *e)
{ (void)h; (void)s; (void)e; return -1; }

static int pg_remove(bb_model_handle_t *h, bb_schema_t *s, const void *key)
{ (void)h; (void)s; (void)key; return -1; }

static int pg_find_all(bb_model_handle_t *h, bb_schema_t *s, void **out_array, size_t *out_count)
{ (void)h; (void)s; (void)out_array; (void)out_count; return -1; }

static int pg_find_first_by_field(bb_model_handle_t *h, bb_schema_t *s, void *out,
                                  const char *field_name, const void *value)
{ (void)h; (void)s; (void)out; (void)field_name; (void)value; return -1; }

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
