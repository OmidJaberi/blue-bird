#include "runner.h"

static int fail_from_driver(const bb_migrate_driver_t *drv,
                            bb_migrate_err_t *err, const char *what)
{
    bb_migrate_err_set(err, "%s: %s", what, drv->last_error);
    return -1;
}

int bb_migrate_plan_for(bb_migrate_driver_t *drv,
                        const bb_migration_set_t *source,
                        bb_migrate_plan_t *out, bb_migrate_err_t *err)
{
    if (!drv || !source || !out)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    bb_migrate_applied_set_t applied = { NULL, 0 };

    if (drv->api->load_applied(drv, &applied) != 0)
        return fail_from_driver(drv, err, "reading migration ledger");

    /* Plan entries copy what they need from `applied`, so it can go. */
    int rc = bb_migrate_plan_build(source, &applied, out, err);
    bb_migrate_applied_set_free(&applied);
    return rc;
}

static void describe_problem(const bb_migrate_plan_t *plan, bb_migrate_err_t *err)
{
    const bb_migrate_entry_t *e = bb_migrate_plan_first_problem(plan);

    if (!e)
        return;

    const char *what = "has an unexpected state";

    switch (e->state)
    {
        case BB_MIGRATE_STATE_MODIFIED:
            what = "was modified after it was applied";
            break;
        case BB_MIGRATE_STATE_MISSING:
            what = "was applied, but its file was not provided";
            break;
        case BB_MIGRATE_STATE_OUT_OF_ORDER:
            what = "is older than the latest applied migration but was never applied";
            break;
        default:
            break;
    }

    if (plan->problem_count > 1)
    {
        bb_migrate_err_set(err, "refusing to migrate: migration %lld (%s) %s (+%zu more problem(s))",
                           (long long)e->version, e->name, what, plan->problem_count - 1);
    }
    else
    {
        bb_migrate_err_set(err, "refusing to migrate: migration %lld (%s) %s",
                           (long long)e->version, e->name, what);
    }
}

static int apply_one(bb_migrate_driver_t *drv, const bb_migration_t *m,
                     bb_migrate_err_t *err)
{
    const bb_migrate_driver_api_t *api = drv->api;

    if (api->begin(drv) != 0)
    {
        bb_migrate_err_set(err, "migration %lld (%s): cannot start transaction: %s",
                           (long long)m->version, m->name, drv->last_error);
        return -1;
    }

    /* Record BEFORE running the SQL. If another process already applied
     * this version, the ledger's primary key rejects the insert here and
     * we roll back without having executed any DDL. */
    if (api->record(drv, m) != 0 || api->exec(drv, m->sql) != 0)
    {
        /* Copy the message out before rollback can overwrite it. */
        bb_migrate_err_set(err, "migration %lld (%s) failed: %s",
                           (long long)m->version, m->name, drv->last_error);
        api->rollback(drv);
        return -1;
    }

    if (api->commit(drv) != 0)
    {
        bb_migrate_err_set(err, "migration %lld (%s): commit failed: %s",
                           (long long)m->version, m->name, drv->last_error);
        api->rollback(drv);
        return -1;
    }

    return 0;
}

int bb_migrate_up(bb_migrate_driver_t *drv,
                  const bb_migration_set_t *source,
                  bb_migrate_progress_cb on_applied, void *user,
                  size_t *applied_count, bb_migrate_err_t *err)
{
    if (applied_count)
        *applied_count = 0;

    if (!drv || !source)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    if (drv->api->ensure_ledger(drv) != 0)
        return fail_from_driver(drv, err, "preparing migration ledger");

    bb_migrate_plan_t plan;

    if (bb_migrate_plan_for(drv, source, &plan, err) != 0)
        return -1;

    if (plan.problem_count > 0)
    {
        describe_problem(&plan, err);
        bb_migrate_plan_free(&plan);
        return -1;
    }

    size_t done = 0;
    int rc = 0;

    for (size_t i = 0; i < plan.count; i++)
    {
        const bb_migrate_entry_t *e = &plan.entries[i];

        if (e->state != BB_MIGRATE_STATE_PENDING)
            continue;

        if (apply_one(drv, e->migration, err) != 0)
        {
            rc = -1;
            break;
        }

        done++;

        if (on_applied)
            on_applied(e->migration, user);
    }

    bb_migrate_plan_free(&plan);

    if (applied_count)
        *applied_count = done;

    return rc;
}
