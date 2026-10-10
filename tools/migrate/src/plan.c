#include "plan.h"

#include <stdlib.h>
#include <string.h>

const char *bb_migrate_state_name(bb_migrate_state_t state)
{
    switch (state)
    {
        case BB_MIGRATE_STATE_APPLIED:      return "applied";
        case BB_MIGRATE_STATE_PENDING:      return "pending";
        case BB_MIGRATE_STATE_MODIFIED:     return "MODIFIED";
        case BB_MIGRATE_STATE_MISSING:      return "MISSING";
        case BB_MIGRATE_STATE_OUT_OF_ORDER: return "OUT-OF-ORDER";
    }

    return "unknown";
}

int bb_migrate_state_is_problem(bb_migrate_state_t state)
{
    return state == BB_MIGRATE_STATE_MODIFIED ||
           state == BB_MIGRATE_STATE_MISSING ||
           state == BB_MIGRATE_STATE_OUT_OF_ORDER;
}

static void add_entry(bb_migrate_plan_t *plan, int64_t version, const char *name,
                      bb_migrate_state_t state, const bb_migration_t *migration)
{
    bb_migrate_entry_t *e = &plan->entries[plan->count++];

    e->version = version;
    strncpy(e->name, name, sizeof e->name - 1);
    e->name[sizeof e->name - 1] = '\0';
    e->state = state;
    e->migration = migration;

    if (state == BB_MIGRATE_STATE_PENDING)
        plan->pending_count++;
    else if (bb_migrate_state_is_problem(state))
        plan->problem_count++;
}

int bb_migrate_plan_build(const bb_migration_set_t *source,
                          const bb_migrate_applied_set_t *applied,
                          bb_migrate_plan_t *out, bb_migrate_err_t *err)
{
    if (!source || !applied || !out)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    memset(out, 0, sizeof *out);

    for (size_t i = 1; i < source->count; i++)
    {
        if (source->items[i].version <= source->items[i - 1].version)
        {
            bb_migrate_err_set(err, "migration source is not sorted by unique version");
            return -1;
        }
    }

    for (size_t j = 1; j < applied->count; j++)
    {
        if (applied->items[j].version <= applied->items[j - 1].version)
        {
            bb_migrate_err_set(err, "applied migration list is not sorted by unique version");
            return -1;
        }
    }

    size_t total = source->count + applied->count;

    if (total == 0)
        return 0;

    out->entries = calloc(total, sizeof *out->entries);

    if (!out->entries)
    {
        bb_migrate_err_set(err, "out of memory");
        return -1;
    }

    int64_t latest_applied = applied->count > 0
        ? applied->items[applied->count - 1].version
        : -1;

    size_t i = 0;
    size_t j = 0;

    /* Merge-join the two sorted lists. */
    while (i < source->count || j < applied->count)
    {
        const bb_migration_t *s = i < source->count ? &source->items[i] : NULL;
        const bb_migrate_applied_t *a = j < applied->count ? &applied->items[j] : NULL;

        if (s && (!a || s->version < a->version))
        {
            /* On disk, never applied. */
            bb_migrate_state_t state = s->version < latest_applied
                ? BB_MIGRATE_STATE_OUT_OF_ORDER
                : BB_MIGRATE_STATE_PENDING;

            add_entry(out, s->version, s->name, state, s);
            i++;
        }
        else if (a && (!s || a->version < s->version))
        {
            /* Applied, but no file for it. */
            add_entry(out, a->version, a->name, BB_MIGRATE_STATE_MISSING, NULL);
            j++;
        }
        else
        {
            bb_migrate_state_t state = strcmp(s->checksum, a->checksum) == 0
                ? BB_MIGRATE_STATE_APPLIED
                : BB_MIGRATE_STATE_MODIFIED;

            add_entry(out, s->version, s->name, state, s);
            i++;
            j++;
        }
    }

    return 0;
}

void bb_migrate_plan_free(bb_migrate_plan_t *plan)
{
    if (!plan)
        return;

    free(plan->entries);
    memset(plan, 0, sizeof *plan);
}

const bb_migrate_entry_t *bb_migrate_plan_first_problem(const bb_migrate_plan_t *plan)
{
    if (!plan)
        return NULL;

    for (size_t i = 0; i < plan->count; i++)
    {
        if (bb_migrate_state_is_problem(plan->entries[i].state))
            return &plan->entries[i];
    }

    return NULL;
}
