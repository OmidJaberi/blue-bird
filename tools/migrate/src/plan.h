#ifndef BB_MIGRATE_PLAN_H
#define BB_MIGRATE_PLAN_H

#include "driver.h"
#include "migration.h"

/* ---------------------------
 * Planner
 *
 * A pure function: (migrations on disk) x (migrations recorded in the
 * ledger) -> one entry per version, each tagged with its state. No I/O,
 * no database -- which is what makes the safety rules below cheap to
 * test exhaustively.
 *
 *   APPLIED        in both, checksums match.            fine
 *   PENDING        on disk only, newer than the latest  will be applied
 *                  applied version.
 *   MODIFIED       in both, checksums differ: the file  PROBLEM
 *                  was edited after it was applied.
 *   MISSING        in the ledger only: an applied       PROBLEM
 *                  migration's file isn't in the input.
 *   OUT_OF_ORDER   on disk only, but OLDER than the     PROBLEM
 *                  latest applied version (e.g. a
 *                  branch merged late).
 *
 * The runner refuses to apply anything while a PROBLEM exists. That is
 * the whole point of the ledger: history is append-only, and the tool
 * says so loudly instead of guessing.
 * --------------------------- */

typedef enum {
    BB_MIGRATE_STATE_APPLIED,
    BB_MIGRATE_STATE_PENDING,
    BB_MIGRATE_STATE_MODIFIED,
    BB_MIGRATE_STATE_MISSING,
    BB_MIGRATE_STATE_OUT_OF_ORDER
} bb_migrate_state_t;

typedef struct {
    int64_t version;
    char name[BB_MIGRATE_NAME_MAX];
    bb_migrate_state_t state;

    /* The source migration, or NULL for MISSING. Points into the
     * bb_migration_set_t the plan was built from, which must outlive
     * the plan. */
    const bb_migration_t *migration;
} bb_migrate_entry_t;

typedef struct {
    bb_migrate_entry_t *entries; /* sorted by version, ascending */
    size_t count;
    size_t pending_count;
    size_t problem_count;
} bb_migrate_plan_t;

/* Both inputs must be sorted by strictly ascending version (the loader
 * and drivers guarantee this); otherwise returns -1. */
int bb_migrate_plan_build(const bb_migration_set_t *source,
                          const bb_migrate_applied_set_t *applied,
                          bb_migrate_plan_t *out, bb_migrate_err_t *err);

void bb_migrate_plan_free(bb_migrate_plan_t *plan);

const char *bb_migrate_state_name(bb_migrate_state_t state);
int bb_migrate_state_is_problem(bb_migrate_state_t state);

/* First PROBLEM entry in version order, or NULL if the plan is clean. */
const bb_migrate_entry_t *bb_migrate_plan_first_problem(const bb_migrate_plan_t *plan);

#endif //BB_MIGRATE_PLAN_H
