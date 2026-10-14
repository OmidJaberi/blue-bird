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
 * The user writes those by hand, which is the point: a tool that guesses
 * at a rename destroys a column.
 *
 * The SQL mirrors what Persist's own backend emits for the manifest, so a
 * generated first migration creates exactly the table Persist would have.
 * --------------------------- */

#define BB_MIGRATE_GEN_OK       0   /* *sql_out holds SQL (caller frees)        */
#define BB_MIGRATE_GEN_NOTHING  1   /* schema already matches; *sql_out == NULL */
#define BB_MIGRATE_GEN_REFUSED -1   /* can't generate; `err` lists every reason */

int bb_migrate_generate_sql(const bb_migrate_table_spec_t *expected,
                            const bb_migrate_table_spec_t *actual,
                            char **sql_out, bb_migrate_err_t *err);

#endif //BB_MIGRATE_GENERATE_H
