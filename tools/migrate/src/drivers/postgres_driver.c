#include "postgres_driver.h"

#include <blue-bird/utils/time.h>
#include <blue-bird/utils/uuid.h>

#include <libpq-fe.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEDGER_TABLE "bb_schema_migrations"

/* Advisory lock around creating the ledger ("bbmigrat" in ASCII). */
#define MIGRATE_LOCK_KEY "7089349062909124980"

typedef struct {
    bb_migrate_driver_t base; /* must be first */
    PGconn *conn;
    char scratch_schema[64];  /* set only for a scratch driver: dropped on close */
} pg_driver_t;

/* ---------------------------
 * Errors and statements
 * --------------------------- */

static void trim_trailing_newlines(char *s)
{
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static void set_error(pg_driver_t *d, const char *context, const char *message)
{
    if (context)
        snprintf(d->base.last_error, sizeof d->base.last_error, "%s: %s", context, message);
    else
        snprintf(d->base.last_error, sizeof d->base.last_error, "%s", message);

    trim_trailing_newlines(d->base.last_error);
}

/* The server's own message for a failed statement, without the "ERROR:  "
 * prefix, and the connection's message if the statement never got that far. */
static const char *result_message(pg_driver_t *d, PGresult *res)
{
    const char *primary = res ? PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY) : NULL;

    if (primary && *primary)
        return primary;

    const char *conn_msg = PQerrorMessage(d->conn);
    return conn_msg && *conn_msg ? conn_msg : "unknown error";
}

/* Runs statements that take no parameters; we don't look at result rows. */
static int run(pg_driver_t *d, const char *sql)
{
    PGresult *res = PQexec(d->conn, sql);
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
    {
        set_error(d, NULL, result_message(d, res));
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;
}

/* Runs a parameterised statement; returns the result (caller clears it) or
 * NULL with last_error set. */
static PGresult *query(pg_driver_t *d, const char *context, const char *sql,
                       int nparams, const char *const *params)
{
    PGresult *res = PQexecParams(d->conn, sql, nparams, NULL, params, NULL, NULL, 0);
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
    {
        set_error(d, context, result_message(d, res));
        PQclear(res);
        return NULL;
    }

    return res;
}

static void copy_text(char *dst, size_t cap, const char *src)
{
    snprintf(dst, cap, "%s", src ? src : "");
}

/* ROLLBACK outside a transaction makes the server warn; only send it when one
 * is open (including a failed one). */
static void rollback_if_open(pg_driver_t *d)
{
    if (PQtransactionStatus(d->conn) != PQTRANS_IDLE)
        PQclear(PQexec(d->conn, "ROLLBACK"));
}

/* ---------------------------
 * Driver operations
 * --------------------------- */

static void drv_close(bb_migrate_driver_t *base)
{
    pg_driver_t *d = (pg_driver_t *)base;

    if (d->conn)
    {
        if (d->scratch_schema[0] != '\0' && PQstatus(d->conn) == CONNECTION_OK)
        {
            char sql[160];

            /* A failed migration may have left a transaction open. */
            rollback_if_open(d);

            snprintf(sql, sizeof sql, "DROP SCHEMA IF EXISTS \"%s\" CASCADE", d->scratch_schema);
            PQclear(PQexec(d->conn, sql));
        }

        PQfinish(d->conn);
    }

    free(d);
}

static int drv_ensure_ledger(bb_migrate_driver_t *base)
{
    pg_driver_t *d = (pg_driver_t *)base;

    /* Two runners starting on a fresh database could both try to create the
     * ledger, and CREATE TABLE IF NOT EXISTS is not safe against that: the
     * loser fails with a duplicate-key error on the catalog. Serialise. */
    if (run(d, "SELECT pg_advisory_lock(" MIGRATE_LOCK_KEY ")") != 0)
        return -1;

    int rc = run(d,
        "CREATE TABLE IF NOT EXISTS " LEDGER_TABLE " ("
        "  version    BIGINT PRIMARY KEY,"
        "  name       TEXT   NOT NULL,"
        "  checksum   TEXT   NOT NULL,"
        "  applied_at BIGINT NOT NULL"
        ")");

    char saved[BB_MIGRATE_ERR_MAX];
    snprintf(saved, sizeof saved, "%s", d->base.last_error);

    run(d, "SELECT pg_advisory_unlock(" MIGRATE_LOCK_KEY ")");

    if (rc != 0)
        snprintf(d->base.last_error, sizeof d->base.last_error, "%s", saved);

    return rc;
}

static int ledger_exists(pg_driver_t *d, int *exists)
{
    PGresult *res = query(d, "checking for ledger",
                          "SELECT to_regclass('" LEDGER_TABLE "') IS NOT NULL", 0, NULL);

    if (!res)
        return -1;

    *exists = PQntuples(res) == 1 && strcmp(PQgetvalue(res, 0, 0), "t") == 0;
    PQclear(res);
    return 0;
}

static int drv_load_applied(bb_migrate_driver_t *base, bb_migrate_applied_set_t *out)
{
    pg_driver_t *d = (pg_driver_t *)base;

    out->items = NULL;
    out->count = 0;

    int exists = 0;

    if (ledger_exists(d, &exists) != 0)
        return -1;

    if (!exists)
        return 0; /* fresh database: nothing applied, nothing created */

    PGresult *res = query(d, "reading ledger",
                          "SELECT version, name, checksum, applied_at FROM " LEDGER_TABLE
                          " ORDER BY version", 0, NULL);

    if (!res)
        return -1;

    int rows = PQntuples(res);

    if (rows > 0)
    {
        out->items = calloc((size_t)rows, sizeof *out->items);

        if (!out->items)
        {
            snprintf(d->base.last_error, sizeof d->base.last_error, "out of memory");
            PQclear(res);
            return -1;
        }
    }

    for (int i = 0; i < rows; i++)
    {
        bb_migrate_applied_t *a = &out->items[out->count++];

        a->version = strtoll(PQgetvalue(res, i, 0), NULL, 10);
        copy_text(a->name, sizeof a->name, PQgetvalue(res, i, 1));
        copy_text(a->checksum, sizeof a->checksum, PQgetvalue(res, i, 2));
        a->applied_at = strtoll(PQgetvalue(res, i, 3), NULL, 10);
    }

    PQclear(res);
    return 0;
}

static int drv_describe_table(bb_migrate_driver_t *base, const char *table,
                              bb_migrate_table_spec_t *out)
{
    pg_driver_t *d = (pg_driver_t *)base;

    memset(out, 0, sizeof *out);
    snprintf(out->table, sizeof out->table, "%s", table);

    /* Straight from the catalog, so it works whatever schema the table is in
     * (to_regclass follows search_path) and the table name is a bound
     * parameter. quote_ident() keeps the name case-sensitive, because
     * Persist's PostgreSQL backend quotes every identifier. A table that
     * doesn't exist gives no rows. */
    PGresult *res = query(d, "inspecting table",
        "SELECT a.attname,"
        "       upper(format_type(a.atttypid, a.atttypmod)),"
        "       EXISTS (SELECT 1 FROM pg_index i"
        "               WHERE i.indrelid = a.attrelid AND i.indisprimary"
        "                 AND a.attnum = ANY (i.indkey))"
        "  FROM pg_attribute a"
        " WHERE a.attrelid = to_regclass(quote_ident($1))"
        "   AND a.attnum > 0 AND NOT a.attisdropped"
        " ORDER BY a.attnum",
        1, (const char *const[]){ table });

    if (!res)
        return -1;

    int rows = PQntuples(res);

    if (rows > BB_MIGRATE_COLUMNS_MAX)
    {
        snprintf(d->base.last_error, sizeof d->base.last_error,
                 "table '%s' has more than %d columns", table, BB_MIGRATE_COLUMNS_MAX);
        PQclear(res);
        return -1;
    }

    for (int i = 0; i < rows; i++)
    {
        bb_migrate_column_t *c = &out->columns[out->count++];

        copy_text(c->name, sizeof c->name, PQgetvalue(res, i, 0));
        copy_text(c->type, sizeof c->type, PQgetvalue(res, i, 1));
        c->primary_key = strcmp(PQgetvalue(res, i, 2), "t") == 0;
    }

    PQclear(res);
    return 0;
}

static int drv_begin(bb_migrate_driver_t *base)
{
    /* No lock here: the runner records the version in the ledger before it
     * runs the migration's SQL, and the ledger's primary key makes two
     * runners applying the same version impossible. The second one to reach
     * the ledger waits for the first one's transaction and then fails with a
     * duplicate-key error, as with the SQLite driver. */
    return run((pg_driver_t *)base, "BEGIN");
}

static int drv_record(bb_migrate_driver_t *base, const bb_migration_t *m)
{
    pg_driver_t *d = (pg_driver_t *)base;
    char version[32], applied_at[32];

    snprintf(version, sizeof version, "%lld", (long long)m->version);
    snprintf(applied_at, sizeof applied_at, "%lld", (long long)bb_time_now_sec());

    const char *params[4] = { version, m->name, m->checksum, applied_at };

    PGresult *res = query(d, "recording migration",
        "INSERT INTO " LEDGER_TABLE " (version, name, checksum, applied_at)"
        " VALUES ($1::bigint, $2, $3, $4::bigint)", 4, params);

    if (!res)
        return -1;

    PQclear(res);
    return 0;
}

static int drv_exec(bb_migrate_driver_t *base, const char *sql)
{
    return run((pg_driver_t *)base, sql);
}

static int drv_commit(bb_migrate_driver_t *base)
{
    return run((pg_driver_t *)base, "COMMIT");
}

static int drv_rollback(bb_migrate_driver_t *base)
{
    /* Without touching last_error: the caller is reporting the real one. */
    rollback_if_open((pg_driver_t *)base);
    return 0;
}

static const bb_migrate_driver_api_t postgres_api = {
    .name          = "postgres",
    .dialect       = BB_MIGRATE_DIALECT_POSTGRES,
    .close         = drv_close,
    .ensure_ledger = drv_ensure_ledger,
    .load_applied  = drv_load_applied,
    .describe_table = drv_describe_table,
    .begin         = drv_begin,
    .record        = drv_record,
    .exec          = drv_exec,
    .commit        = drv_commit,
    .rollback      = drv_rollback,
};

/* ---------------------------
 * Opening
 * --------------------------- */

static pg_driver_t *connect_to(const char *conninfo, bb_migrate_err_t *err)
{
    if (!conninfo || !*conninfo)
    {
        bb_migrate_err_set(err, "no database given");
        return NULL;
    }

    pg_driver_t *d = calloc(1, sizeof *d);

    if (!d)
    {
        bb_migrate_err_set(err, "out of memory");
        return NULL;
    }

    d->base.api = &postgres_api;
    d->conn = PQconnectdb(conninfo);

    if (!d->conn || PQstatus(d->conn) != CONNECTION_OK)
    {
        /* The message only: the conninfo may contain a password. */
        char msg[BB_MIGRATE_ERR_MAX];

        snprintf(msg, sizeof msg, "%s", d->conn ? PQerrorMessage(d->conn) : "out of memory");
        trim_trailing_newlines(msg);

        bb_migrate_err_set(err, "cannot connect to PostgreSQL: %s", msg);
        drv_close(&d->base);
        return NULL;
    }

    /* "relation already exists, skipping" and similar are expected when
     * migrations use IF NOT EXISTS; keep stderr for real problems. */
    PQclear(PQexec(d->conn, "SET client_min_messages TO warning"));
    return d;
}

bb_migrate_driver_t *bb_migrate_postgres_open(const char *conninfo, bb_migrate_err_t *err)
{
    pg_driver_t *d = connect_to(conninfo, err);

    return d ? &d->base : NULL;
}

bb_migrate_driver_t *bb_migrate_postgres_open_scratch(const char *conninfo, bb_migrate_err_t *err)
{
    pg_driver_t *d = connect_to(conninfo, err);

    if (!d)
        return NULL;

    /* A random name made of letters, digits and underscores: safe to put
     * in quotes without escaping. */
    char id[BB_UUID_BUF_LEN];

    if (bb_uuid_v4_string(id) != 0)
    {
        bb_migrate_err_set(err, "cannot generate a scratch schema name");
        drv_close(&d->base);
        return NULL;
    }

    char name[sizeof d->scratch_schema];
    size_t n = (size_t)snprintf(name, sizeof name, "bb_migrate_scratch_");

    for (const char *p = id; *p && n + 1 < sizeof name; p++)
        if (*p != '-')
            name[n++] = *p;
    name[n] = '\0';

    char sql[160];

    snprintf(sql, sizeof sql, "CREATE SCHEMA \"%s\"", name);

    if (run(d, sql) != 0)
    {
        bb_migrate_err_set(err, "cannot create a scratch schema (needs permission to create schemas): %s",
                           d->base.last_error);
        drv_close(&d->base);
        return NULL;
    }

    /* From here on drv_close() drops it again. */
    snprintf(d->scratch_schema, sizeof d->scratch_schema, "%s", name);

    snprintf(sql, sizeof sql, "SET search_path TO \"%s\"", name);

    if (run(d, sql) != 0)
    {
        bb_migrate_err_set(err, "cannot use the scratch schema: %s", d->base.last_error);
        drv_close(&d->base);
        return NULL;
    }

    return &d->base;
}
