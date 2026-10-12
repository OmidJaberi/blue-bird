#include "driver.h"
#include "drivers/sqlite_driver.h"
#include "migration.h"
#include "plan.h"
#include "runner.h"

#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <string.h>

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static int table_exists(bb_migrate_driver_t *d, const char *table)
{
    bb_migrate_table_spec_t spec;
    BB_ASSERT(d->api->describe_table(d, table, &spec) == 0);
    return spec.count > 0;
}

/* Versions currently recorded in the ledger, in order, e.g. "1,3". */
static void ledger_versions(bb_migrate_driver_t *d, char *out, size_t cap)
{
    bb_migrate_applied_set_t applied;
    BB_ASSERT(d->api->load_applied(d, &applied) == 0);

    out[0] = '\0';

    for (size_t i = 0; i < applied.count; i++)
    {
        char one[32];
        snprintf(one, sizeof one, "%s%lld", i ? "," : "", (long long)applied.items[i].version);
        strncat(out, one, cap - strlen(out) - 1);
    }

    bb_migrate_applied_set_free(&applied);
}

static void test_branch_merged_late(void)
{
    printf("\tTesting a branch merged late: refused by default, applied with the option...\n");

    write_text_file("0001_base.sql", "CREATE TABLE base (id INTEGER);\n");
    write_text_file("0002_late_branch.sql", "CREATE TABLE late_branch (id INTEGER);\n");
    write_text_file("0003_main_branch.sql", "CREATE TABLE main_branch (id INTEGER);\n");

    bb_migrate_err_t err;
    bb_migrate_driver_t *d = bb_migrate_sqlite_open(":memory:", &err);
    BB_ASSERT(d != NULL);

    /* The database has seen 1 and 3 (main shipped without the other branch). */
    const char *first[] = { "0001_base.sql", "0003_main_branch.sql" };
    bb_migration_set_t set;
    BB_ASSERT(bb_migrate_load_files(first, 2, &set, &err) == 0);
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, NULL, &err) == 0);
    bb_migration_set_free(&set);

    char versions[64];
    ledger_versions(d, versions, sizeof versions);
    BB_ASSERT(strcmp(versions, "1,3") == 0);

    /* Now the other branch lands: 0002 shows up. */
    const char *all[] = { "0001_base.sql", "0002_late_branch.sql", "0003_main_branch.sql" };
    BB_ASSERT(bb_migrate_load_files(all, 3, &set, &err) == 0);

    /* Default: refused, nothing changes, and the message says how to proceed. */
    size_t applied = 99;
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &applied, &err) != 0);
    BB_ASSERT(applied == 0);
    BB_ASSERT(strstr(err.msg, "migration 2 (late_branch)") != NULL);
    BB_ASSERT(strstr(err.msg, "--allow-out-of-order") != NULL);
    BB_ASSERT(!table_exists(d, "late_branch"));

    /* status with the option: it is pending, but still labelled late. */
    bb_migrate_options_t opts = { 1 };
    bb_migrate_plan_t plan;
    BB_ASSERT(bb_migrate_plan_for(d, &set, &opts, &plan, &err) == 0);
    BB_ASSERT(plan.problem_count == 0 && plan.pending_count == 1);
    BB_ASSERT(plan.entries[1].version == 2 && plan.entries[1].late == 1);
    bb_migrate_plan_free(&plan);

    /* With the option: only the late one runs. */
    BB_ASSERT(bb_migrate_up(d, &set, &opts, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 1);
    BB_ASSERT(table_exists(d, "late_branch"));
    ledger_versions(d, versions, sizeof versions);
    BB_ASSERT(strcmp(versions, "1,2,3") == 0);

    /* Afterwards it is an ordinary applied migration: default options are fine again. */
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 0);

    bb_migration_set_free(&set);
    bb_migrate_driver_close(d);

    remove("0001_base.sql");
    remove("0002_late_branch.sql");
    remove("0003_main_branch.sql");
}

int main(void)
{
    printf("Running migration out-of-order integration tests...\n");

    test_branch_merged_late();

    printf("All migration out-of-order tests passed!\n");
    return 0;
}
