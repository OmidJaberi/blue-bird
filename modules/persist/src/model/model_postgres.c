#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>

#include "blue-bird/persist/model/model_postgres.h"

/* ---------------------------
 * Step 1: connection lifecycle only.
 *
 * open()/close() are real; the CRUD callbacks below are placeholders that
 * fail cleanly (-1) so the backend can already be registered and selected
 * without silently pretending to persist anything.
 *
 * Single PGconn for now. A PGconn must not be used by two threads at once,
 * so a pool (like the SQLite backend's) comes before any concurrent use.
 * --------------------------- */

typedef struct {
    PGconn *conn;
} BB_ModelPostgresHandle;

static bb_model_handle_t *pg_open(const char *uri)
{
    if (!uri || uri[0] == '\0')
        return NULL;

    BB_ModelPostgresHandle *h = calloc(1, sizeof(*h));
    if (!h)
        return NULL;

    h->conn = PQconnectdb(uri);
    if (!h->conn)
    {
        free(h);
        return NULL;
    }

    if (PQstatus(h->conn) != CONNECTION_OK)
    {
        /* Message only; never print the conninfo, it may contain a password. */
        fprintf(stderr, "bb postgres: connection failed: %s", PQerrorMessage(h->conn));
        PQfinish(h->conn);
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
    free(h);
}

static int pg_insert(bb_model_handle_t *h, bb_schema_t *s, void *e)
{ (void)h; (void)s; (void)e; return -1; }

static int pg_find_by_pk(bb_model_handle_t *h, bb_schema_t *s, void *out, const void *key)
{ (void)h; (void)s; (void)out; (void)key; return -1; }

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
