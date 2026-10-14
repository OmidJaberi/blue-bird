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

/* ---------------------------
 * Structured differences
 *
 * Each difference between what the schema expects and what the database
 * has, as data. The drift check prints them as text; the generator turns
 * the safe ones into SQL.
 * --------------------------- */

typedef enum {
    BB_MIGRATE_DIFF_TABLE_MISSING,  /* table doesn't exist at all                 */
    BB_MIGRATE_DIFF_COLUMN_MISSING, /* in the schema, not in the database         */
    BB_MIGRATE_DIFF_COLUMN_EXTRA,   /* in the database, not in the schema         */
    BB_MIGRATE_DIFF_TYPE_MISMATCH,  /* in both, different types                   */
    BB_MIGRATE_DIFF_PK_MISSING,     /* schema says primary key, database doesn't  */
    BB_MIGRATE_DIFF_PK_EXTRA        /* database says primary key, schema doesn't  */
} bb_migrate_diff_kind_t;

typedef struct {
    bb_migrate_diff_kind_t kind;
    const char *table;
    const char *column;        /* NULL for TABLE_MISSING */
    const char *expected_type; /* from the schema, where relevant, else NULL */
    const char *actual_type;   /* from the database, where relevant, else NULL */
    int expected_pk;           /* COLUMN_MISSING: is the missing column a primary key */
} bb_migrate_diff_t;

/* Pointers inside the diff are only valid during the callback. */
typedef void (*bb_migrate_diff_visit_cb)(const bb_migrate_diff_t *diff, void *user);

/* Reports every difference, in a stable order (schema columns first, then
 * database-only columns). Returns the number of differences. An `actual`
 * with zero columns means "table missing". Column order is ignored. */
size_t bb_migrate_spec_diff(const bb_migrate_table_spec_t *expected,
                            const bb_migrate_table_spec_t *actual,
                            bb_migrate_diff_visit_cb on_diff, void *user);

/* One-line, human-readable description of a difference. */
void bb_migrate_diff_describe(const bb_migrate_diff_t *diff, char *buf, size_t cap);

/* Called once per difference with a printable one-line description. */
typedef void (*bb_migrate_diff_cb)(const char *message, void *user);

/* bb_migrate_spec_diff() + bb_migrate_diff_describe(): reports each
 * difference as a printable line. Returns the number of differences
 * (0 means they match). */
size_t bb_migrate_spec_compare(const bb_migrate_table_spec_t *expected,
                               const bb_migrate_table_spec_t *actual,
                               bb_migrate_diff_cb on_diff, void *user);

#endif //BB_MIGRATE_TABLE_SPEC_H
