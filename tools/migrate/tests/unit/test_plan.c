#include "plan.h"

#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <string.h>

/* Builds in-memory sets; no files or database involved. */

static void set_migration(bb_migration_t *m, int64_t version, const char *name, const char *checksum)
{
    memset(m, 0, sizeof *m);
    m->version = version;
    snprintf(m->name, sizeof m->name, "%s", name);
    snprintf(m->checksum, sizeof m->checksum, "%s", checksum);
}

static void set_applied(bb_migrate_applied_t *a, int64_t version, const char *name, const char *checksum)
{
    memset(a, 0, sizeof *a);
    a->version = version;
    snprintf(a->name, sizeof a->name, "%s", name);
    snprintf(a->checksum, sizeof a->checksum, "%s", checksum);
}

static void test_empty(void)
{
    printf("\tTesting empty inputs...\n");

    bb_migration_set_t src = { NULL, 0 };
    bb_migrate_applied_set_t app = { NULL, 0 };
    bb_migrate_plan_t plan;

    BB_ASSERT(bb_migrate_plan_build(&src, &app, &plan, NULL) == 0);
    BB_ASSERT(plan.count == 0 && plan.pending_count == 0 && plan.problem_count == 0);
    BB_ASSERT(bb_migrate_plan_first_problem(&plan) == NULL);
    bb_migrate_plan_free(&plan);
}

static void test_fresh_database_all_pending(void)
{
    printf("\tTesting fresh database -> everything pending...\n");

    bb_migration_t m[2];
    set_migration(&m[0], 1, "a", "h1");
    set_migration(&m[1], 2, "b", "h2");

    bb_migration_set_t src = { m, 2 };
    bb_migrate_applied_set_t app = { NULL, 0 };
    bb_migrate_plan_t plan;

    BB_ASSERT(bb_migrate_plan_build(&src, &app, &plan, NULL) == 0);
    BB_ASSERT(plan.count == 2 && plan.pending_count == 2 && plan.problem_count == 0);
    BB_ASSERT(plan.entries[0].state == BB_MIGRATE_STATE_PENDING);
    BB_ASSERT(plan.entries[0].migration == &m[0]);
    BB_ASSERT(plan.entries[1].version == 2);
    bb_migrate_plan_free(&plan);
}

static void test_partially_applied(void)
{
    printf("\tTesting partially applied -> only the new one pending...\n");

    bb_migration_t m[3];
    set_migration(&m[0], 1, "a", "h1");
    set_migration(&m[1], 2, "b", "h2");
    set_migration(&m[2], 3, "c", "h3");

    bb_migrate_applied_t a[2];
    set_applied(&a[0], 1, "a", "h1");
    set_applied(&a[1], 2, "b", "h2");

    bb_migration_set_t src = { m, 3 };
    bb_migrate_applied_set_t app = { a, 2 };
    bb_migrate_plan_t plan;

    BB_ASSERT(bb_migrate_plan_build(&src, &app, &plan, NULL) == 0);
    BB_ASSERT(plan.entries[0].state == BB_MIGRATE_STATE_APPLIED);
    BB_ASSERT(plan.entries[1].state == BB_MIGRATE_STATE_APPLIED);
    BB_ASSERT(plan.entries[2].state == BB_MIGRATE_STATE_PENDING);
    BB_ASSERT(plan.pending_count == 1 && plan.problem_count == 0);
    bb_migrate_plan_free(&plan);
}

static void test_modified(void)
{
    printf("\tTesting modified migration is a problem...\n");

    bb_migration_t m[2];
    set_migration(&m[0], 1, "a", "CHANGED");
    set_migration(&m[1], 2, "b", "h2");

    bb_migrate_applied_t a[1];
    set_applied(&a[0], 1, "a", "h1");

    bb_migration_set_t src = { m, 2 };
    bb_migrate_applied_set_t app = { a, 1 };
    bb_migrate_plan_t plan;

    BB_ASSERT(bb_migrate_plan_build(&src, &app, &plan, NULL) == 0);
    BB_ASSERT(plan.entries[0].state == BB_MIGRATE_STATE_MODIFIED);
    BB_ASSERT(plan.problem_count == 1 && plan.pending_count == 1);
    BB_ASSERT(bb_migrate_plan_first_problem(&plan) == &plan.entries[0]);
    bb_migrate_plan_free(&plan);
}

static void test_missing(void)
{
    printf("\tTesting applied-but-missing file is a problem...\n");

    bb_migration_t m[1];
    set_migration(&m[0], 2, "b", "h2");

    bb_migrate_applied_t a[2];
    set_applied(&a[0], 1, "gone", "h1");
    set_applied(&a[1], 2, "b", "h2");

    bb_migration_set_t src = { m, 1 };
    bb_migrate_applied_set_t app = { a, 2 };
    bb_migrate_plan_t plan;

    BB_ASSERT(bb_migrate_plan_build(&src, &app, &plan, NULL) == 0);
    BB_ASSERT(plan.entries[0].state == BB_MIGRATE_STATE_MISSING);
    BB_ASSERT(plan.entries[0].migration == NULL);
    BB_ASSERT(strcmp(plan.entries[0].name, "gone") == 0);
    BB_ASSERT(plan.problem_count == 1);
    bb_migrate_plan_free(&plan);
}

static void test_out_of_order(void)
{
    printf("\tTesting late-arriving older migration is a problem...\n");

    bb_migration_t m[3];
    set_migration(&m[0], 1, "a", "h1");
    set_migration(&m[1], 2, "late", "h2");   /* never applied, but 3 already is */
    set_migration(&m[2], 3, "c", "h3");

    bb_migrate_applied_t a[2];
    set_applied(&a[0], 1, "a", "h1");
    set_applied(&a[1], 3, "c", "h3");

    bb_migration_set_t src = { m, 3 };
    bb_migrate_applied_set_t app = { a, 2 };
    bb_migrate_plan_t plan;

    BB_ASSERT(bb_migrate_plan_build(&src, &app, &plan, NULL) == 0);
    BB_ASSERT(plan.entries[1].version == 2);
    BB_ASSERT(plan.entries[1].state == BB_MIGRATE_STATE_OUT_OF_ORDER);
    BB_ASSERT(plan.pending_count == 0 && plan.problem_count == 1);
    bb_migrate_plan_free(&plan);
}

static void test_unsorted_input_rejected(void)
{
    printf("\tTesting unsorted input is rejected...\n");

    bb_migration_t m[2];
    set_migration(&m[0], 2, "b", "h2");
    set_migration(&m[1], 1, "a", "h1");

    bb_migration_set_t src = { m, 2 };
    bb_migrate_applied_set_t app = { NULL, 0 };
    bb_migrate_plan_t plan;
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_plan_build(&src, &app, &plan, &err) != 0);
    BB_ASSERT(err.msg[0] != '\0');
}

static void test_state_names(void)
{
    printf("\tTesting state helpers...\n");

    BB_ASSERT(!bb_migrate_state_is_problem(BB_MIGRATE_STATE_APPLIED));
    BB_ASSERT(!bb_migrate_state_is_problem(BB_MIGRATE_STATE_PENDING));
    BB_ASSERT(bb_migrate_state_is_problem(BB_MIGRATE_STATE_MODIFIED));
    BB_ASSERT(bb_migrate_state_is_problem(BB_MIGRATE_STATE_MISSING));
    BB_ASSERT(bb_migrate_state_is_problem(BB_MIGRATE_STATE_OUT_OF_ORDER));
    BB_ASSERT(strcmp(bb_migrate_state_name(BB_MIGRATE_STATE_PENDING), "pending") == 0);
}

int main(void)
{
    printf("Running migration planner unit tests...\n");

    test_empty();
    test_fresh_database_all_pending();
    test_partially_applied();
    test_modified();
    test_missing();
    test_out_of_order();
    test_unsorted_input_rejected();
    test_state_names();

    printf("All migration planner tests passed!\n");
    return 0;
}
