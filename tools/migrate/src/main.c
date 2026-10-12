#include "driver.h"
#include "drivers/sqlite_driver.h"
#include "migration.h"
#include "plan.h"
#include "runner.h"
#include "table_spec.h"

#include <stdio.h>
#include <string.h>

#define MAX_SCHEMAS 16

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s status --db <database> [--allow-out-of-order] <migration.sql> [...]\n"
        "  %s up     --db <database> [--allow-out-of-order] <migration.sql> [...]\n"
        "  %s verify --schema <manifest.json> [--schema ...] <migration.sql> [...]\n"
        "\n"
        "  status   lists every migration and whether it is applied, pending,\n"
        "           or a problem. Read-only. Exits 1 if any problem is found.\n"
        "  up       applies pending migrations in version order, each in its\n"
        "           own transaction. Refuses to run if status shows a problem.\n"
        "  verify   applies all migrations to a scratch in-memory database and\n"
        "           checks that each persist.schema manifest's table matches the\n"
        "           result (columns, types, primary key). Touches no real\n"
        "           database. Exits 1 on any difference. Meant for CI.\n"
        "\n"
        "  --allow-out-of-order  apply a migration whose version is older than the\n"
        "           latest applied one (e.g. a branch merged late) instead of\n"
        "           treating it as a problem. It runs against a schema that already\n"
        "           has the newer migrations, so make sure it is compatible.\n"
        "\n"
        "Migration files are named <version>_<name>.sql (e.g. 0002_add_email.sql).\n"
        "Pass them by expanding the glob yourself, e.g.:\n"
        "  %s up --db app.db migrations/*.sql\n"
        "Pass the complete set every time: an applied migration whose file\n"
        "is missing from the list is reported as a problem.\n",
        argv0, argv0, argv0, argv0);
}

static void on_applied(const bb_migration_t *m, void *user)
{
    (void)user;
    printf("applied  %lld  %s\n", (long long)m->version, m->name);
}

static void on_diff(const char *message, void *user)
{
    (void)user;
    printf("  - %s\n", message);
}

static int cmd_status(bb_migrate_driver_t *drv, const bb_migration_set_t *set,
                      const bb_migrate_options_t *opts)
{
    bb_migrate_err_t err;
    bb_migrate_plan_t plan;

    if (bb_migrate_plan_for(drv, set, opts, &plan, &err) != 0)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        return 1;
    }

    printf("%-10s %-13s %s\n", "VERSION", "STATE", "NAME");

    for (size_t i = 0; i < plan.count; i++)
    {
        const bb_migrate_entry_t *e = &plan.entries[i];

        printf("%-10lld %-13s %s%s\n", (long long)e->version,
               bb_migrate_state_name(e->state), e->name,
               (e->late && e->state == BB_MIGRATE_STATE_PENDING) ? "  (out of order)" : "");
    }

    printf("\n%zu pending, %zu problem(s)\n", plan.pending_count, plan.problem_count);

    int rc = plan.problem_count > 0 ? 1 : 0;
    bb_migrate_plan_free(&plan);
    return rc;
}

static int cmd_up(bb_migrate_driver_t *drv, const bb_migration_set_t *set,
                  const bb_migrate_options_t *opts)
{
    bb_migrate_err_t err;
    size_t applied = 0;

    int rc = bb_migrate_up(drv, set, opts, on_applied, NULL, &applied, &err);

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

static int cmd_verify(const char *const *schemas, size_t schema_count,
                      const bb_migration_set_t *set)
{
    bb_migrate_err_t err;
    bb_migrate_driver_t *scratch = bb_migrate_sqlite_open(":memory:", &err);

    if (!scratch)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        return 1;
    }

    int rc = 1;
    size_t total = 0;

    if (bb_migrate_up(scratch, set, NULL, NULL, NULL, NULL, &err) != 0)
    {
        fprintf(stderr, "bb-migrate: migrations do not apply cleanly to an empty database: %s\n",
                err.msg);
        goto done;
    }

    for (size_t i = 0; i < schema_count; i++)
    {
        bb_migrate_table_spec_t expected;
        size_t diffs = 0;

        if (bb_migrate_spec_from_manifest(schemas[i], &expected, &err) != 0)
        {
            fprintf(stderr, "bb-migrate: %s\n", err.msg);
            goto done;
        }

        printf("%s (table '%s'):\n", schemas[i], expected.table);

        if (bb_migrate_check_table(scratch, &expected, on_diff, NULL, &diffs, &err) != 0)
        {
            fprintf(stderr, "bb-migrate: %s\n", err.msg);
            goto done;
        }

        if (diffs == 0)
            printf("  ok\n");

        total += diffs;
    }

    if (total == 0)
    {
        printf("\nschemas match migrations\n");
        rc = 0;
    }
    else
    {
        printf("\n%zu difference(s) between schemas and migrations\n", total);
    }

done:
    bb_migrate_driver_close(scratch);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        print_usage(argv[0]);
        return 2;
    }

    const char *command = argv[1];

    if (strcmp(command, "verify") == 0)
    {
        const char *schemas[MAX_SCHEMAS];
        size_t schema_count = 0;
        int i = 2;

        while (i + 1 < argc && strcmp(argv[i], "--schema") == 0)
        {
            if (schema_count == MAX_SCHEMAS)
            {
                fprintf(stderr, "bb-migrate: too many --schema options (max %d)\n", MAX_SCHEMAS);
                return 2;
            }

            schemas[schema_count++] = argv[i + 1];
            i += 2;
        }

        if (schema_count == 0 || i >= argc)
        {
            fprintf(stderr, "bb-migrate: verify needs --schema <manifest> and at least one migration file\n\n");
            print_usage(argv[0]);
            return 2;
        }

        bb_migrate_err_t err;
        bb_migration_set_t set;

        if (bb_migrate_load_files((const char *const *)&argv[i], (size_t)(argc - i), &set, &err) != 0)
        {
            fprintf(stderr, "bb-migrate: %s\n", err.msg);
            return 1;
        }

        int rc = cmd_verify(schemas, schema_count, &set);
        bb_migration_set_free(&set);
        return rc;
    }

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
    bb_migrate_options_t opts = { 0 };
    int first_file = 4;

    while (first_file < argc && strcmp(argv[first_file], "--allow-out-of-order") == 0)
    {
        opts.allow_out_of_order = 1;
        first_file++;
    }

    if (first_file >= argc)
    {
        fprintf(stderr, "bb-migrate: no migration files given\n\n");
        print_usage(argv[0]);
        return 2;
    }

    const char *const *files = (const char *const *)&argv[first_file];
    size_t file_count = (size_t)(argc - first_file);

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

    int rc = is_up ? cmd_up(drv, &set, &opts) : cmd_status(drv, &set, &opts);

    bb_migrate_driver_close(drv);
    bb_migration_set_free(&set);
    return rc;
}
