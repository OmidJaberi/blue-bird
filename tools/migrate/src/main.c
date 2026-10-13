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
        "  %s status --db <database> [options] [migration.sql ...]\n"
        "  %s up     --db <database> [options] [migration.sql ...]\n"
        "  %s verify --schema <manifest.json> [--schema ...] [options] [migration.sql ...]\n"
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
        "options:\n"
        "  --dir <directory>     use every *.sql file in the directory (in addition to\n"
        "                        any files listed). A file whose name starts with a\n"
        "                        digit but doesn't end in .sql is an error, so a\n"
        "                        mistyped extension can't silently skip a migration.\n"
        "  --allow-out-of-order  (status, up) apply a migration whose version is older\n"
        "                        than the latest applied one (e.g. a branch merged late)\n"
        "                        instead of treating it as a problem. It runs against a\n"
        "                        schema that already has the newer migrations, so make\n"
        "                        sure it is compatible.\n"
        "\n"
        "Migration files are named <version>_<name>.sql (e.g. 0002_add_email.sql).\n"
        "Give the directory, or expand a glob yourself:\n"
        "  %s up --db app.db --dir migrations\n"
        "  %s up --db app.db migrations/*.sql\n"
        "Provide the complete set every time: an applied migration whose file\n"
        "is missing is reported as a problem.\n",
        argv0, argv0, argv0, argv0, argv0);
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

typedef struct {
    const char *command;
    const char *db;
    const char *dir;
    const char *schemas[MAX_SCHEMAS];
    size_t schema_count;
    bb_migrate_options_t opts;
    int first_file; /* index in argv of the first positional migration file */
} cli_args_t;

/* Returns 0, or 2 (usage error, already reported). */
static int parse_args(int argc, char **argv, cli_args_t *a)
{
    memset(a, 0, sizeof *a);
    a->command = argv[1];

    int is_up = strcmp(a->command, "up") == 0;
    int is_status = strcmp(a->command, "status") == 0;
    int is_verify = strcmp(a->command, "verify") == 0;

    if (!is_up && !is_status && !is_verify)
    {
        fprintf(stderr, "bb-migrate: unknown command '%s'\n\n", a->command);
        return 2;
    }

    int i = 2;

    while (i < argc && strncmp(argv[i], "--", 2) == 0)
    {
        const char *opt = argv[i];

        if (strcmp(opt, "--allow-out-of-order") == 0)
        {
            if (is_verify)
            {
                fprintf(stderr, "bb-migrate: --allow-out-of-order does not apply to verify\n\n");
                return 2;
            }

            a->opts.allow_out_of_order = 1;
            i++;
            continue;
        }

        int is_db = strcmp(opt, "--db") == 0;
        int is_dir = strcmp(opt, "--dir") == 0;
        int is_schema = strcmp(opt, "--schema") == 0;

        if (!is_db && !is_dir && !is_schema)
        {
            fprintf(stderr, "bb-migrate: unknown option '%s'\n\n", opt);
            return 2;
        }

        if (i + 1 >= argc)
        {
            fprintf(stderr, "bb-migrate: %s needs a value\n\n", opt);
            return 2;
        }

        const char *value = argv[i + 1];

        if (is_db && is_verify)
        {
            fprintf(stderr, "bb-migrate: verify uses a scratch database; --db does not apply\n\n");
            return 2;
        }

        if (is_schema && !is_verify)
        {
            fprintf(stderr, "bb-migrate: --schema only applies to verify\n\n");
            return 2;
        }

        if ((is_db && a->db) || (is_dir && a->dir))
        {
            fprintf(stderr, "bb-migrate: %s given more than once\n\n", opt);
            return 2;
        }

        if (is_db)
            a->db = value;
        else if (is_dir)
            a->dir = value;
        else
        {
            if (a->schema_count == MAX_SCHEMAS)
            {
                fprintf(stderr, "bb-migrate: too many --schema options (max %d)\n", MAX_SCHEMAS);
                return 2;
            }

            a->schemas[a->schema_count++] = value;
        }

        i += 2;
    }

    a->first_file = i;

    if (!is_verify && !a->db)
    {
        fprintf(stderr, "bb-migrate: %s needs --db <database>\n\n", a->command);
        return 2;
    }

    if (is_verify && a->schema_count == 0)
    {
        fprintf(stderr, "bb-migrate: verify needs at least one --schema <manifest>\n\n");
        return 2;
    }

    if (!a->dir && a->first_file >= argc)
    {
        fprintf(stderr, "bb-migrate: no migrations given (use --dir or list files)\n\n");
        return 2;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        print_usage(argv[0]);
        return 2;
    }

    cli_args_t args;

    if (parse_args(argc, argv, &args) != 0)
    {
        print_usage(argv[0]);
        return 2;
    }

    bb_migrate_err_t err;
    bb_migration_set_t set;
    char **paths = NULL;
    size_t path_count = 0;

    if (bb_migrate_collect_paths(args.dir,
                                 (const char *const *)&argv[args.first_file],
                                 (size_t)(argc - args.first_file),
                                 &paths, &path_count, &err) != 0)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        return 1;
    }

    int load_rc = bb_migrate_load_files((const char *const *)paths, path_count, &set, &err);
    bb_migrate_paths_free(paths, path_count);

    if (load_rc != 0)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        return 1;
    }

    int rc;

    if (strcmp(args.command, "verify") == 0)
    {
        rc = cmd_verify(args.schemas, args.schema_count, &set);
    }
    else
    {
        bb_migrate_driver_t *drv = bb_migrate_sqlite_open(args.db, &err);

        if (!drv)
        {
            fprintf(stderr, "bb-migrate: %s\n", err.msg);
            bb_migration_set_free(&set);
            return 1;
        }

        rc = strcmp(args.command, "up") == 0
            ? cmd_up(drv, &set, &args.opts)
            : cmd_status(drv, &set, &args.opts);

        bb_migrate_driver_close(drv);
    }

    bb_migration_set_free(&set);
    return rc;
}
