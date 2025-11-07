#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include "persist/persist.h"
#include "persist/persist_sqlite.h"

/* =========================================================================
 *  SQLite Handle
 * ========================================================================= */

struct PersistHandle {
    sqlite3 *db;
};

/* =========================================================================
 *  Internal Helpers
 * ========================================================================= */

static int persist_sqlite_exec(sqlite3 *db, const char *sql)
{
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) sqlite3_free(errmsg);
        return 1; //PERSIST_ERR_DB
    }
    return 0; //PERSIST_OK
}

/* =========================================================================
 *  Backend API Implementation
 * ========================================================================= */

static PersistHandle *sqlite_open(const char *uri)
{
    if (!uri)
        return NULL;

    PersistHandle *h = calloc(1, sizeof(*h));
    if (!h)
        return NULL;

    int rc = sqlite3_open(uri, &h->db);
    if (rc != SQLITE_OK) {
        sqlite3_close(h->db);
        free(h);
        return NULL;
    }

    /* Create table if it doesn't exist */
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS kv ("
        "key TEXT PRIMARY KEY,"
        "value BLOB"
        ");";
    persist_sqlite_exec(h->db, create_sql);

    return h;
}

static void sqlite_close(PersistHandle *h)
{
    if (!h)
        return;
    sqlite3_close(h->db);
    free(h);
}

static int sqlite_save(PersistHandle *h,
                       const char *key,
                       const void *data,
                       size_t size)
{
    if (!h || !key || !data)
        return 1; //PERSIST_ERR_PARAM

    const char *sql = "INSERT OR REPLACE INTO kv (key, value) VALUES (?, ?);";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return 1; //PERSIST_ERR_DB

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, data, (int)size, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 /*PERSIST_OK*/ : 1 /*PERSIST_ERR_DB*/;
}

static int sqlite_load(PersistHandle *h,
                       const char *key,
                       void *buf,
                       size_t bufsize)
{
    if (!h || !key || !buf)
        return 1; //ERSIST_ERR_PARAM

    const char *sql = "SELECT value FROM kv WHERE key = ?;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return 1; //PERSIST_ERR_DB

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    int result = 1; //PERSIST_ERR_KEY

    if (rc == SQLITE_ROW) {
        const void *data = sqlite3_column_blob(stmt, 0);
        int len = sqlite3_column_bytes(stmt, 0);
        if ((size_t)len <= bufsize) {
            memcpy(buf, data, len);
            result = 0; //PERSIST_OK
        } else {
            result = 1; //PERSIST_ERR_IO  (buffer too small)
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

static int sqlite_remove(PersistHandle *h, const char *key)
{
    if (!h || !key)
        return 1; //PERSIST_ERR_PARAM

    const char *sql = "DELETE FROM kv WHERE key = ?;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(h->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return 1; //PERSIST_ERR_DB

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 /*PERSIST_OK*/ : 1 /*PERSIST_ERR_DB*/;
}

/* =========================================================================
 *  API Registration
 * ========================================================================= */

static const PersistAPI sqlite_api = {
    .name   = "sqlite",
    .open   = sqlite_open,
    .close  = sqlite_close,
    .save   = sqlite_save,
    .load   = sqlite_load,
    .remove = sqlite_remove,
};

int persist_sqlite_register(void)
{
    return persist_register(&sqlite_api);
}

