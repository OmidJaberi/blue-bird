#ifndef BB_MIGRATE_MIGRATION_H
#define BB_MIGRATE_MIGRATION_H

#include "migrate_err.h"

#include <stddef.h>
#include <stdint.h>

/* ---------------------------
 * Migration source layer
 *
 * A migration is one SQL file named  <version>_<name>.sql  e.g.
 *
 *     0001_create_users.sql
 *     0002_add_user_email.sql
 *
 * <version> is a non-negative integer that defines apply order (compare
 * numerically, so 0002 < 10). <name> is [A-Za-z0-9_-]+. Zero-padded
 * counters and timestamps (20261009120000_...) both work.
 *
 * This layer is pure "files in, ordered migrations out": it knows
 * nothing about databases, ledgers or what has already been applied.
 * --------------------------- */

#define BB_MIGRATE_NAME_MAX 128
#define BB_MIGRATE_CHECKSUM_LEN 64 /* hex chars of SHA-256, excluding NUL */

typedef struct {
    int64_t version;
    char name[BB_MIGRATE_NAME_MAX];
    char checksum[BB_MIGRATE_CHECKSUM_LEN + 1];
    char *sql; /* NUL-terminated file contents; owned by the set */
} bb_migration_t;

typedef struct {
    bb_migration_t *items; /* sorted by version, strictly ascending */
    size_t count;
} bb_migration_set_t;

/* Splits "dir/0002_add_email.sql" into version=2, name="add_email".
 * Only the final path component is looked at. Returns 0 or -1. */
int bb_migrate_parse_filename(const char *path, int64_t *version,
                              char *name, size_t name_cap,
                              bb_migrate_err_t *err);

/* Lowercase-hex SHA-256 of `data`, with CRLF treated as LF so a file
 * checked out with different line endings doesn't look "modified". */
void bb_migrate_checksum(const char *data, size_t len,
                         char out[BB_MIGRATE_CHECKSUM_LEN + 1]);

/* Builds the list of migration file paths to load from a directory
 * and/or explicit files, so callers don't depend on shell globbing.
 *
 *  - `dir` (may be NULL): every entry ending in ".sql" is included, as
 *    "<dir>/<name>". Other files (README.md, ...) are ignored, EXCEPT an
 *    entry whose name starts with a digit but doesn't end in ".sql"
 *    (e.g. "0003_add_index.SQL"): that is almost certainly a migration
 *    that would otherwise be silently skipped, so it is an error.
 *  - `files` are appended as given.
 *
 * If `dir` is given and yields no .sql files and `files` is empty, that
 * is an error too (a mistyped directory shouldn't look like "nothing to
 * do"). The result is owned by the caller: bb_migrate_paths_free(). */
int bb_migrate_collect_paths(const char *dir,
                             const char *const *files, size_t file_count,
                             char ***out_paths, size_t *out_count,
                             bb_migrate_err_t *err);

void bb_migrate_paths_free(char **paths, size_t count);

/* Loads every file, sorts by version, rejects duplicate versions.
 * `paths` order doesn't matter (shell globs sort by locale; we sort by
 * number). On success the caller owns `out` and must call
 * bb_migration_set_free(). On failure `out` is left empty. */
int bb_migrate_load_files(const char *const *paths, size_t count,
                          bb_migration_set_t *out, bb_migrate_err_t *err);

void bb_migration_set_free(bb_migration_set_t *set);

#endif //BB_MIGRATE_MIGRATION_H
