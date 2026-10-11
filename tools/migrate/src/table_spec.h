#ifndef BB_MIGRATE_TABLE_SPEC_H
#define BB_MIGRATE_TABLE_SPEC_H

#include "migrate_err.h"
#include "migration.h"

#include <stddef.h>

/* ---------------------------
 * Table spec + drift check
 *
 * Persist creates a table from a persist.schema manifest, but never
 * alters it. Migrations are what evolve it -- so nothing ties the two
 * together except discipline. This module closes that loop: it turns a
 * manifest into the table shape Persist expects, and compares that to
 * the shape a database actually has (after the migrations ran).
 *
 * Pure logic, no database and no I/O beyond reading the manifest file.
 *
 * What is compared mirrors what Persist's SQLite backend really emits
 * (see field_type_to_sql in persist's model_sqlite.c):
 *
 *   - column names
 *   - column type: int -> INTEGER, string/uuid -> TEXT, blob -> BLOB
 *   - which column is the primary key
 *
 * Deliberately NOT compared:
 *   - column order (ALTER TABLE ADD COLUMN always appends)
 *   - size / required / unique (Persist emits no constraints for them)
 * If Persist later starts emitting constraints, extend this module.
 * --------------------------- */

#define BB_MIGRATE_COLUMNS_MAX 64
#define BB_MIGRATE_TYPE_MAX 16

typedef struct {
    char name[BB_MIGRATE_NAME_MAX];
    char type[BB_MIGRATE_TYPE_MAX]; /* upper-case SQL type, e.g. "TEXT" */
    int primary_key;
} bb_migrate_column_t;

typedef struct {
    char table[BB_MIGRATE_NAME_MAX];
    bb_migrate_column_t columns[BB_MIGRATE_COLUMNS_MAX];
    size_t count;
} bb_migrate_table_spec_t;

/* Reads a "persist.schema" manifest. The table is the manifest's
 * "table" (falling back to "name", like bb-codegen). Returns 0 or -1. */
int bb_migrate_spec_from_manifest(const char *path, bb_migrate_table_spec_t *out,
                                  bb_migrate_err_t *err);

/* Called once per difference with a printable one-line description. */
typedef void (*bb_migrate_diff_cb)(const char *message, void *user);

/* Compares what the schema expects with what the database has and
 * reports every difference. Returns the number of differences (0 means
 * they match). An actual spec with zero columns means "table missing". */
size_t bb_migrate_spec_compare(const bb_migrate_table_spec_t *expected,
                               const bb_migrate_table_spec_t *actual,
                               bb_migrate_diff_cb on_diff, void *user);

#endif //BB_MIGRATE_TABLE_SPEC_H
