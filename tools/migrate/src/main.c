#include "driver.h"
#include "drivers/sqlite_driver.h"
#include "migration.h"
#include "plan.h"
#include "runner.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s status --db <database> <migration.sql> [migration2.sql ...]\n"
        "  %s up     --db <database> <migration.sql> [migration2.sql ...]\n"
        "\n"
        "  status   lists every migration and whether it is applied, pending,\n"
        "           or a problem. Read-only. Exits 1 if any problem is found.\n"
        "  up       applies pending migrations in version order, each in its\n"
        "           own transaction. Refuses to run if status shows a problem.\n"
        "\n"
        "Migration files are named <version>_<name>.sql (e.g. 0002_add_email.sql).\n"
        "Pass them by expanding the glob yourself, e.g.:\n"
        "  %s up --db app.db migrations/*.sql\n"
        "Pass the complete set every time: an applied migration whose file\n"
        "is missing from the list is reported as a problem.\n",
        argv0, argv0, argv0);
}

static void on_applied(const bb_migration_t *m, void *user)
{
    (void)user;
    printf("applied  %lld  %s\n", (long long)m->version, m->name);
}

static int cmd_status(bb_migrate_driver_t *drv, const bb_migration_set_t *set)
{
    bb_migrate_err_t err;
    bb_migrate_plan_t plan;

    if (bb_migrate_plan_for(drv, set, &plan, &err) != 0)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        return 1;
    }

    printf("%-10s %-13s %s\n", "VERSION", "STATE", "NAME");

    for (size_t i = 0; i < plan.count; i++)
    {
        const bb_migrate_entry_t *e = &plan.entries[i];

        printf("%-10lld %-13s %s\n", (long long)e->version,
               bb_migrate_state_name(e->state), e->name);
    }

    printf("\n%zu pending, %zu problem(s)\n", plan.pending_count, plan.problem_count);

    int rc = plan.problem_count > 0 ? 1 : 0;
    bb_migrate_plan_free(&plan);
    return rc;
}

static int cmd_up(bb_migrate_driver_t *drv, const bb_migration_set_t *set)
{
    bb_migrate_err_t err;
    size_t applied = 0;

    int rc = bb_migrate_up(drv, set, on_applied, NULL, &applied, &err);

    if (rc != 0)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);

        if (applied > 0)
            fprintf(stderr, "bb-migrate: %zu migration(s) were applied before the failure\n", applied);

        return 1;
    }

    if (applied == 0)
        printf("up to date\n");
    else
        printf("%zu migration(s) applied\n", applied);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        print_usage(argv[0]);
        return 2;
    }

    const char *command = argv[1];
    int is_up;

    if (strcmp(command, "status") == 0)
        is_up = 0;
    else if (strcmp(command, "up") == 0)
        is_up = 1;
    else
    {
        fprintf(stderr, "bb-migrate: unknown command '%s'\n\n", command);
        print_usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[2], "--db") != 0)
    {
        fprintf(stderr, "bb-migrate: expected --db <database>\n\n");
        print_usage(argv[0]);
        return 2;
    }

    const char *db_path = argv[3];
    const char *const *files = (const char *const *)&argv[4];
    size_t file_count = (size_t)(argc - 4);

    bb_migrate_err_t err;
    bb_migration_set_t set;

    if (bb_migrate_load_files(files, file_count, &set, &err) != 0)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        return 1;
    }

    bb_migrate_driver_t *drv = bb_migrate_sqlite_open(db_path, &err);

    if (!drv)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        bb_migration_set_free(&set);
        return 1;
    }

    int rc = is_up ? cmd_up(drv, &set) : cmd_status(drv, &set);

    bb_migrate_driver_close(drv);
    bb_migration_set_free(&set);
    return rc;
}
