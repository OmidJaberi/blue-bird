#ifndef BB_MIGRATE_DRIVER_H
#define BB_MIGRATE_DRIVER_H

#include "migration.h"

#include <stddef.h>
#include <stdint.h>

/* ---------------------------
 * Driver layer
 *
 * The only part of bb-migrate that talks to a database. Everything
 * above it (planner, runner, CLI) is written against this function
 * table, so supporting another database means writing one new driver
 * -- the same shape as Persist's pluggable bb_model_api_t backends.
 *
 * A driver owns a single connection. Deliberately NOT built on
 * Persist's SQLite backend: that one is a connection pool for
 * concurrent CRUD and exposes no transactions, whereas a migration run
 * needs one connection and explicit BEGIN/COMMIT around each step.
 * The two only meet at the database file.
 *
 * The "ledger" is a table the driver maintains (bb_schema_migrations)
 * holding one row per applied migration: version, name, checksum and
 * applied_at. It is the single source of truth for "what version is
 * this database at?".
 *
 * All operations return 0 on success, non-zero on failure, and leave a
 * message in driver->last_error.
 * --------------------------- */

typedef struct {
    int64_t version;
    char name[BB_MIGRATE_NAME_MAX];
    char checksum[BB_MIGRATE_CHECKSUM_LEN + 1];
    int64_t applied_at; /* unix seconds */
} bb_migrate_applied_t;

typedef struct {
    bb_migrate_applied_t *items; /* sorted by version, ascending */
    size_t count;
} bb_migrate_applied_set_t;

void bb_migrate_applied_set_free(bb_migrate_applied_set_t *set);

typedef struct bb_migrate_driver bb_migrate_driver_t;

typedef struct {
    const char *name; /* e.g. "sqlite" */

    void (*close)(bb_migrate_driver_t *d);

    /* Creates the ledger table if it doesn't exist. Idempotent. */
    int (*ensure_ledger)(bb_migrate_driver_t *d);

    /* Reads the ledger. MUST have no side effects: a missing ledger is
     * an empty result, not something to create. This is what lets
     * `status` run safely against any database. */
    int (*load_applied)(bb_migrate_driver_t *d, bb_migrate_applied_set_t *out);

    /* One migration = begin, record, exec, commit (rollback on any
     * failure). Drivers whose database can't roll back DDL should say
     * so in their docs; the runner's contract is unchanged. */
    int (*begin)(bb_migrate_driver_t *d);
    int (*record)(bb_migrate_driver_t *d, const bb_migration_t *m);
    int (*exec)(bb_migrate_driver_t *d, const char *sql);
    int (*commit)(bb_migrate_driver_t *d);
    int (*rollback)(bb_migrate_driver_t *d);
} bb_migrate_driver_api_t;

/* Drivers embed this as the FIRST member of their own struct and cast. */
struct bb_migrate_driver {
    const bb_migrate_driver_api_t *api;
    char last_error[BB_MIGRATE_ERR_MAX];
};

/* Convenience: closes and frees any driver. NULL-safe. */
void bb_migrate_driver_close(bb_migrate_driver_t *d);

#endif //BB_MIGRATE_DRIVER_H
