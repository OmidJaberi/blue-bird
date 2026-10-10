#ifndef BB_MIGRATE_RUNNER_H
#define BB_MIGRATE_RUNNER_H

#include "driver.h"
#include "migration.h"
#include "plan.h"

/* ---------------------------
 * Runner
 *
 * Orchestration only: ask the driver what's applied, ask the planner
 * what to do about it, then apply pending migrations one at a time.
 * No SQL lives here and no database types leak in -- it works with any
 * bb_migrate_driver_t.
 * --------------------------- */

/* Reads the ledger and plans against `source`. Read-only: never creates
 * the ledger or changes the database. The caller frees the plan, and
 * `source` must outlive it. */
int bb_migrate_plan_for(bb_migrate_driver_t *drv,
                        const bb_migration_set_t *source,
                        bb_migrate_plan_t *out, bb_migrate_err_t *err);

/* Called after each migration commits. Optional. */
typedef void (*bb_migrate_progress_cb)(const bb_migration_t *m, void *user);

/* Applies every pending migration in version order.
 *
 *  - Refuses (returns -1, changes nothing) if the plan has any problem.
 *  - Each migration runs in its own transaction together with its
 *    ledger row, so a database is never left half-migrated *within* a
 *    step, and "recorded" always means "fully applied".
 *  - Stops at the first failure. Earlier migrations stay applied; fix
 *    the failing file and run `up` again.
 *
 * `applied_count` (optional) receives how many were committed, even on
 * failure. */
int bb_migrate_up(bb_migrate_driver_t *drv,
                  const bb_migration_set_t *source,
                  bb_migrate_progress_cb on_applied, void *user,
                  size_t *applied_count, bb_migrate_err_t *err);

#endif //BB_MIGRATE_RUNNER_H
