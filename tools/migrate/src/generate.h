#ifndef BB_MIGRATE_GENERATE_H
#define BB_MIGRATE_GENERATE_H

#include "migrate_err.h"
#include "table_spec.h"

/* ---------------------------
 * Migration generator
 *
 * Turns "what the schema expects" vs "what the migrations produced" into
 * the SQL that closes the gap. There is no snapshot of a previous
 * manifest: the migrations ARE the history, and replaying them (as
 * `verify` does) gives the current shape to diff against.
 *
 * Pure: strings in, string out. No database, no files.
 *
 * Only changes that cannot lose data are generated:
 *
 *   table missing        CREATE TABLE IF NOT EXISTS ...
 *   column added         ALTER TABLE ... ADD COLUMN ...
 *
 * Everything else is REFUSED, and nothing is generated for that table:
 *
 *   column in the database but not the schema   might be a drop, might be a
 *                                               rename; can't tell, and
 *                                               either can lose data
 *   column type changed                         needs a table rebuild
 *   primary key changed or added                SQLite can't ALTER one
 *
 * A tool that guesses at a rename destroys a column, so a rename is only
 * generated when the user SAYS it is one, with a bb_migrate_rename_t:
 *
 *   rename `from` -> `to`    ALTER TABLE ... RENAME COLUMN ... TO ...
 *
 * (data preserved; needs SQLite 3.25+). The hint is checked against the
 * real diff: `from` must be in the database but not the schema, `to` in
 * the schema but not the database, with the same type and primary-key
 * flag. A hint that doesn't hold is an error, never silently ignored.
 * Anything else still refused is written by hand.
 *
 * The SQL mirrors what Persist's own backend emits for the manifest, so a
 * generated first migration creates exactly the table Persist would have.
 * --------------------------- */

#define BB_MIGRATE_RENAMES_MAX 32

/* "In table `table`, the database's column `from` is the schema's `to`." */
typedef struct {
    char table[BB_MIGRATE_NAME_MAX];
    char from[BB_MIGRATE_NAME_MAX];
    char to[BB_MIGRATE_NAME_MAX];
} bb_migrate_rename_t;

/* Parses "<table>.<old column>=<new column>" (the --rename argument).
 * All three parts must be plain identifiers. Returns 0 or -1. */
int bb_migrate_parse_rename(const char *arg, bb_migrate_rename_t *out,
                            bb_migrate_err_t *err);

#define BB_MIGRATE_GEN_OK       0   /* *sql_out holds SQL (caller frees)        */
#define BB_MIGRATE_GEN_NOTHING  1   /* schema already matches; *sql_out == NULL */
#define BB_MIGRATE_GEN_REFUSED -1   /* can't generate; `err` lists every reason */

/* `renames` may be NULL. Hints for other tables are ignored here (the
 * caller checks that every hint matched some table). */
int bb_migrate_generate_sql(const bb_migrate_table_spec_t *expected,
                            const bb_migrate_table_spec_t *actual,
                            const bb_migrate_rename_t *renames, size_t rename_count,
                            char **sql_out, bb_migrate_err_t *err);

#endif //BB_MIGRATE_GENERATE_H
