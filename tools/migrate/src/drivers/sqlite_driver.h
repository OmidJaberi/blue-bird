#ifndef BB_MIGRATE_SQLITE_DRIVER_H
#define BB_MIGRATE_SQLITE_DRIVER_H

#include "../driver.h"

/* Opens (creating if needed) the SQLite database at `path` -- or
 * ":memory:" -- and returns a driver for it, or NULL with `err` set.
 * Release with bb_migrate_driver_close().
 *
 * SQLite can roll back DDL, so a failed migration leaves no trace. Each
 * migration runs inside BEGIN IMMEDIATE ... COMMIT, which also makes a
 * second concurrent `up` wait (up to 5s) instead of interleaving. The
 * migration SQL must therefore not contain its own BEGIN/COMMIT. */
bb_migrate_driver_t *bb_migrate_sqlite_open(const char *path, bb_migrate_err_t *err);

#endif //BB_MIGRATE_SQLITE_DRIVER_H
