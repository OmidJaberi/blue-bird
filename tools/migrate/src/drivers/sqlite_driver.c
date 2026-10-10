#include "sqlite_driver.h"

#include <blue-bird/utils/time.h>

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEDGER_TABLE "bb_schema_migrations"
#define BUSY_TIMEOUT_MS 5000

typedef struct {
    bb_migrate_driver_t base; /* must be first */
    sqlite3 *db;
} sqlite_driver_t;

static void set_error_from_db(sqlite_driver_t *d, const char *context)
{
    snprintf(d->base.last_error, sizeof d->base.last_error,
             "%s: %s", context, sqlite3_errmsg(d->db));
}

/* Runs a statement with no parameters and no result rows we care about. */
static int run(sqlite_driver_t *d, const char *sql)
{
    char *msg = NULL;

    if (sqlite3_exec(d->db, sql, NULL, NULL, &msg) != SQLITE_OK)
    {
        snprintf(d->base.last_error, sizeof d->base.last_error,
                 "%s", msg ? msg : sqlite3_errmsg(d->db));
        sqlite3_free(msg);
        return -1;
    }

    return 0;
}

/* ---------------------------
 * Driver operations
 * --------------------------- */

static void drv_close(bb_migrate_driver_t *base)
{
    sqlite_driver_t *d = (sqlite_driver_t *)base;

    if (d->db)
        sqlite3_close(d->db);

    free(d);
}

static int drv_ensure_ledger(bb_migrate_driver_t *base)
{
    return run((sqlite_driver_t *)base,
        "CREATE TABLE IF NOT EXISTS " LEDGER_TABLE " ("
        "  version    INTEGER PRIMARY KEY,"
        "  name       TEXT    NOT NULL,"
        "  checksum   TEXT    NOT NULL,"
        "  applied_at INTEGER NOT NULL"
        ")");
}

static int ledger_exists(sqlite_driver_t *d, int *exists)
{
    sqlite3_stmt *st = NULL;

    if (sqlite3_prepare_v2(d->db,
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = '" LEDGER_TABLE "'",
            -1, &st, NULL) != SQLITE_OK)
    {
        set_error_from_db(d, "checking for ledger");
        return -1;
    }

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
    {
        set_error_from_db(d, "checking for ledger");
        return -1;
    }

    *exists = (rc == SQLITE_ROW);
    return 0;
}

static void copy_text(char *dst, size_t cap, const unsigned char *src)
{
    snprintf(dst, cap, "%s", src ? (const char *)src : "");
}

static int drv_load_applied(bb_migrate_driver_t *base, bb_migrate_applied_set_t *out)
{
    sqlite_driver_t *d = (sqlite_driver_t *)base;

    out->items = NULL;
    out->count = 0;

    int exists = 0;

    if (ledger_exists(d, &exists) != 0)
        return -1;

    if (!exists)
        return 0; /* fresh database: nothing applied, nothing created */

    sqlite3_stmt *st = NULL;

    if (sqlite3_prepare_v2(d->db,
            "SELECT version, name, checksum, applied_at FROM " LEDGER_TABLE
            " ORDER BY version",
            -1, &st, NULL) != SQLITE_OK)
    {
        set_error_from_db(d, "reading ledger");
        return -1;
    }

    size_t cap = 0;
    int rc;

    while ((rc = sqlite3_step(st)) == SQLITE_ROW)
    {
        if (out->count == cap)
        {
            size_t new_cap = cap ? cap * 2 : 16;
            bb_migrate_applied_t *grown = realloc(out->items, new_cap * sizeof *grown);

            if (!grown)
            {
                snprintf(d->base.last_error, sizeof d->base.last_error, "out of memory");
                sqlite3_finalize(st);
                bb_migrate_applied_set_free(out);
                return -1;
            }

            out->items = grown;
            cap = new_cap;
        }

        bb_migrate_applied_t *a = &out->items[out->count++];

        a->version = sqlite3_column_int64(st, 0);
        copy_text(a->name, sizeof a->name, sqlite3_column_text(st, 1));
        copy_text(a->checksum, sizeof a->checksum, sqlite3_column_text(st, 2));
        a->applied_at = sqlite3_column_int64(st, 3);
    }

    sqlite3_finalize(st);

    if (rc != SQLITE_DONE)
    {
        set_error_from_db(d, "reading ledger");
        bb_migrate_applied_set_free(out);
        return -1;
    }

    return 0;
}

static int drv_begin(bb_migrate_driver_t *base)
{
    /* IMMEDIATE takes the write lock now, so concurrent runners queue up
     * behind the busy timeout rather than failing midway. */
    return run((sqlite_driver_t *)base, "BEGIN IMMEDIATE");
}

static int drv_record(bb_migrate_driver_t *base, const bb_migration_t *m)
{
    sqlite_driver_t *d = (sqlite_driver_t *)base;
    sqlite3_stmt *st = NULL;

    if (sqlite3_prepare_v2(d->db,
            "INSERT INTO " LEDGER_TABLE " (version, name, checksum, applied_at)"
            " VALUES (?, ?, ?, ?)",
            -1, &st, NULL) != SQLITE_OK)
    {
        set_error_from_db(d, "recording migration");
        return -1;
    }

    sqlite3_bind_int64(st, 1, m->version);
    sqlite3_bind_text(st, 2, m->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, m->checksum, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, bb_time_now_sec());

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE)
    {
        set_error_from_db(d, "recording migration");
        return -1;
    }

    return 0;
}

static int drv_exec(bb_migrate_driver_t *base, const char *sql)
{
    return run((sqlite_driver_t *)base, sql);
}

static int drv_commit(bb_migrate_driver_t *base)
{
    return run((sqlite_driver_t *)base, "COMMIT");
}

static int drv_rollback(bb_migrate_driver_t *base)
{
    sqlite_driver_t *d = (sqlite_driver_t *)base;

    /* Rolling back when no transaction is open (e.g. BEGIN itself
     * failed) is harmless; don't clobber the real error with it. */
    sqlite3_exec(d->db, "ROLLBACK", NULL, NULL, NULL);
    return 0;
}

static const bb_migrate_driver_api_t sqlite_api = {
    .name          = "sqlite",
    .close         = drv_close,
    .ensure_ledger = drv_ensure_ledger,
    .load_applied  = drv_load_applied,
    .begin         = drv_begin,
    .record        = drv_record,
    .exec          = drv_exec,
    .commit        = drv_commit,
    .rollback      = drv_rollback,
};

bb_migrate_driver_t *bb_migrate_sqlite_open(const char *path, bb_migrate_err_t *err)
{
    if (!path)
    {
        bb_migrate_err_set(err, "no database path given");
        return NULL;
    }

    sqlite_driver_t *d = calloc(1, sizeof *d);

    if (!d)
    {
        bb_migrate_err_set(err, "out of memory");
        return NULL;
    }

    d->base.api = &sqlite_api;

    if (sqlite3_open_v2(path, &d->db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
    {
        bb_migrate_err_set(err, "cannot open database '%s': %s", path,
                           d->db ? sqlite3_errmsg(d->db) : "out of memory");
        drv_close(&d->base);
        return NULL;
    }

    sqlite3_busy_timeout(d->db, BUSY_TIMEOUT_MS);
    return &d->base;
}
