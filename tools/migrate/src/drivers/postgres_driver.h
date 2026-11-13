#ifndef BB_MIGRATE_POSTGRES_DRIVER_H
#define BB_MIGRATE_POSTGRES_DRIVER_H

#include "../driver.h"

/* Connects to a PostgreSQL database (a libpq URI such as
 * "postgresql://user:pass@host:5432/db", or key/value conninfo) and returns a
 * driver for it, or NULL with `err` set. Release with
 * bb_migrate_driver_close().
 *
 * The ledger (bb_schema_migrations) and every migration's objects live in the
 * first schema of the connection's search_path, like Persist's PostgreSQL
 * backend, which also uses unqualified names.
 *
 * PostgreSQL rolls back DDL, so a failed migration leaves no trace. Each
 * migration runs inside BEGIN ... COMMIT, with its ledger row inserted first.
 * Two simultaneous `up` runs therefore never both apply a version: the
 * second waits for the first's transaction, then fails with a duplicate-key
 * error (the same as the SQLite driver). The migration SQL must not contain
 * its own BEGIN/COMMIT, nor statements PostgreSQL refuses inside a
 * transaction (CREATE INDEX CONCURRENTLY, ALTER TYPE ... ADD VALUE, ...). */
bb_migrate_driver_t *bb_migrate_postgres_open(const char *conninfo, bb_migrate_err_t *err);

/* Like bb_migrate_postgres_open(), but for throwaway work (verify, generate):
 * creates a uniquely named schema, points the connection's search_path at it
 * alone, and drops it again (CASCADE) when the driver is closed. Nothing else in
 * the database is touched, and migrations written with unqualified names
 * land in the scratch schema.
 *
 * The user needs permission to create schemas. If the process is killed
 * before closing, the schema (named bb_migrate_scratch_<random>) is left
 * behind and can simply be dropped. */
bb_migrate_driver_t *bb_migrate_postgres_open_scratch(const char *conninfo, bb_migrate_err_t *err);

#endif //BB_MIGRATE_POSTGRES_DRIVER_H
