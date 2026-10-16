#include "driver.h"
#include "generate.h"
#include "drivers/sqlite_driver.h"
#include "migration.h"
#include "plan.h"
#include "runner.h"
#include "table_spec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SCHEMAS 16

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s status --db <database> [options] [migration.sql ...]\n"
        "  %s up     --db <database> [options] [migration.sql ...]\n"
        "  %s verify   --schema <manifest.json> [--schema ...] [options] [migration.sql ...]\n"
        "  %s generate --schema <manifest.json> [--schema ...] --dir <directory> --name <name> [--rename <table>.<old>=<new> ...] [--dry-run]\n"
        "\n"
        "  status   lists every migration and whether it is applied, pending,\n"
        "           or a problem. Read-only. Exits 1 if any problem is found.\n"
        "  up       applies pending migrations in version order, each in its\n"
        "           own transaction. Refuses to run if status shows a problem.\n"
        "  verify   applies all migrations to a scratch in-memory database and\n"
        "           checks that each persist.schema manifest's table matches the\n"
        "           result (columns, types, primary key). Touches no real\n"
        "           database. Exits 1 on any difference. Meant for CI.\n"
        "  generate writes the next migration (<version>_<name>.sql) into --dir so that\n"
        "           the migrations produce what each manifest expects. It replays the\n"
        "           existing migrations, diffs against the manifest, and only generates\n"
        "           changes that cannot lose data: a missing table, or a new column.\n"
        "           A dropped or retyped column, or a primary key change, is refused\n"
        "           with an explanation and nothing is written: write those by hand.\n"
        "           A rename can't be told from a drop, so say so with --rename.\n"
        "           --dry-run prints the SQL without writing a file.\n"
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
        "  --name <name>         (generate) the migration's name, e.g. add_due\n"
        "  --rename <t>.<a>=<b>  (generate) in table t, column a was renamed to b\n"
        "                        (ALTER TABLE ... RENAME COLUMN, data preserved, SQLite\n"
        "                        3.25+). Repeatable. Must match the real difference.\n"
        "  --dry-run             (generate) print the SQL, write nothing\n"
        "\n"
        "Give the directory, or expand a glob yourself:\n"
        "  %s up --db app.db --dir migrations\n"
        "  %s up --db app.db migrations/*.sql\n"
        "Provide the complete set every time: an applied migration whose file\n"
        "is missing is reported as a problem.\n",
        argv0, argv0, argv0, argv0, argv0, argv0);
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
    const char *name;
    const char *schemas[MAX_SCHEMAS];
    size_t schema_count;
    bb_migrate_rename_t renames[BB_MIGRATE_RENAMES_MAX];
    size_t rename_count;
    int dry_run;
    bb_migrate_options_t opts;
    int first_file; /* index in argv of the first positional migration file */
} cli_args_t;

/* ---------------------------
 * generate
 * --------------------------- */

static const char *base_name(const char *path)
{
    const char *base = path;

    for (const char *p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }

    return base;
}

static int append_text(char **buf, size_t *len, const char *text)
{
    size_t add = strlen(text);
    char *grown = realloc(*buf, *len + add + 1);

    if (!grown)
        return -1;

    memcpy(grown + *len, text, add + 1);
    *buf = grown;
    *len += add;
    return 0;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;

    fclose(f);
    return 1;
}

static int write_new_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");

    if (!f)
        return -1;

    size_t len = strlen(content);
    int ok = fwrite(content, 1, len, f) == len;

    return (fclose(f) == 0 && ok) ? 0 : -1;
}

static int cmd_generate(const cli_args_t *args, const bb_migration_set_t *set)
{
    bb_migrate_err_t err;

    /* Name the new file first: a bad --name should fail before any work. */
    int64_t version = bb_migrate_next_version(set);
    char file[BB_MIGRATE_NAME_MAX + 64];

    if (bb_migrate_format_filename(set, version, args->name, file, sizeof file, &err) != 0)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        return 1;
    }

    size_t dir_len = strlen(args->dir);
    int has_sep = dir_len > 0 && (args->dir[dir_len - 1] == '/' || args->dir[dir_len - 1] == '\\');
    size_t path_cap = dir_len + strlen(file) + 2;
    char *path = malloc(path_cap);

    if (!path)
    {
        fprintf(stderr, "bb-migrate: out of memory\n");
        return 1;
    }

    snprintf(path, path_cap, "%s%s%s", args->dir, has_sep ? "" : "/", file);

    bb_migrate_driver_t *scratch = bb_migrate_sqlite_open(":memory:", &err);
    char *content = NULL;
    size_t content_len = 0;
    int rc = 1;

    if (!scratch)
    {
        fprintf(stderr, "bb-migrate: %s\n", err.msg);
        goto done;
    }

    if (bb_migrate_up(scratch, set, NULL, NULL, NULL, NULL, &err) != 0)
    {
        fprintf(stderr, "bb-migrate: existing migrations do not apply cleanly to an empty database: %s\n",
                err.msg);
        goto done;
    }

    /* Header, then one section per table that needs changes. */
    char header[BB_MIGRATE_ERR_MAX * 2] = "-- Generated by bb-migrate from: ";

    for (size_t i = 0; i < args->schema_count; i++)
    {
        if (i > 0)
            strncat(header, ", ", sizeof header - strlen(header) - 1);

        strncat(header, base_name(args->schemas[i]), sizeof header - strlen(header) - 1);
    }

    strncat(header, "\n-- Review before committing, then run `bb-migrate verify` to confirm.\n\n",
            sizeof header - strlen(header) - 1);

    if (append_text(&content, &content_len, header) != 0)
        goto oom;

    size_t sections = 0;
    size_t refused = 0;
    size_t body_start = content_len;
    unsigned char hint_matched[BB_MIGRATE_RENAMES_MAX] = { 0 };

    for (size_t i = 0; i < args->schema_count; i++)
    {
        bb_migrate_table_spec_t expected;
        bb_migrate_table_spec_t actual;
        char *sql = NULL;

        if (bb_migrate_spec_from_manifest(args->schemas[i], &expected, &err) != 0)
        {
            fprintf(stderr, "bb-migrate: %s\n", err.msg);
            goto done;
        }

        if (scratch->api->describe_table(scratch, expected.table, &actual) != 0)
        {
            fprintf(stderr, "bb-migrate: inspecting table: %s\n", scratch->last_error);
            goto done;
        }

        for (size_t r = 0; r < args->rename_count; r++)
        {
            if (strcmp(args->renames[r].table, expected.table) == 0)
                hint_matched[r] = 1;
        }

        int gen = bb_migrate_generate_sql(&expected, &actual, args->renames, args->rename_count,
                                          &sql, &err);

        if (gen == BB_MIGRATE_GEN_REFUSED)
        {
            fprintf(stderr, "bb-migrate: %s: %s\n", args->schemas[i], err.msg);
            refused++;
            continue;
        }

        if (gen == BB_MIGRATE_GEN_NOTHING)
        {
            printf("table '%s' already matches its migrations\n", expected.table);
            continue;
        }

        char section[BB_MIGRATE_NAME_MAX + 32];
        snprintf(section, sizeof section, "-- table '%s'\n", expected.table);

        int bad = append_text(&content, &content_len, section) != 0 ||
                  append_text(&content, &content_len, sql) != 0 ||
                  append_text(&content, &content_len, "\n") != 0;

        free(sql);

        if (bad)
            goto oom;

        sections++;
    }

    /* A hint for a table no --schema defines is a typo, not something to ignore. */
    for (size_t r = 0; r < args->rename_count; r++)
    {
        if (!hint_matched[r])
        {
            fprintf(stderr, "bb-migrate: --rename %s.%s=%s: none of the --schema manifests defines table '%s'\n",
                    args->renames[r].table, args->renames[r].from, args->renames[r].to,
                    args->renames[r].table);
            refused++;
        }
    }

    if (refused > 0)
    {
        fprintf(stderr, "bb-migrate: nothing was written\n");
        goto done;
    }

    if (sections == 0)
    {
        printf("schemas already match migrations; nothing to generate\n");
        rc = 0;
        goto done;
    }

    /* Self-check: the generated SQL must run, and must leave every table
     * matching its manifest. Otherwise it is never written. */
    if (scratch->api->exec(scratch, content + body_start) != 0)
    {
        fprintf(stderr, "bb-migrate: the generated SQL failed on a scratch database (not written): %s\n",
                scratch->last_error);
        goto done;
    }

    for (size_t i = 0; i < args->schema_count; i++)
    {
        bb_migrate_table_spec_t expected;
        size_t diffs = 0;

        if (bb_migrate_spec_from_manifest(args->schemas[i], &expected, &err) != 0 ||
            bb_migrate_check_table(scratch, &expected, NULL, NULL, &diffs, &err) != 0)
        {
            fprintf(stderr, "bb-migrate: %s\n", err.msg);
            goto done;
        }

        if (diffs > 0)
        {
            fprintf(stderr, "bb-migrate: the generated migration does not fully reconcile "
                            "'%s' with its manifest (not written)\n", expected.table);
            goto done;
        }
    }

    if (args->dry_run)
    {
        printf("would write %s:\n\n%s", path, content);
        rc = 0;
        goto done;
    }

    if (file_exists(path))
    {
        fprintf(stderr, "bb-migrate: '%s' already exists; refusing to overwrite\n", path);
        goto done;
    }

    if (write_new_file(path, content) != 0)
    {
        fprintf(stderr, "bb-migrate: cannot write '%s'\n", path);
        goto done;
    }

    printf("wrote %s\n\n%s", path, content);
    rc = 0;
    goto done;

oom:
    fprintf(stderr, "bb-migrate: out of memory\n");

done:
    free(content);
    free(path);

    if (scratch)
        bb_migrate_driver_close(scratch);

    return rc;
}

static int usage_error(const char *fmt, const char *arg)
{
    fprintf(stderr, "bb-migrate: ");
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n\n");
    return 2;
}

/* Returns 0, or 2 (usage error, already reported). */
static int parse_args(int argc, char **argv, cli_args_t *a)
{
    memset(a, 0, sizeof *a);
    a->command = argv[1];

    int is_up = strcmp(a->command, "up") == 0;
    int is_status = strcmp(a->command, "status") == 0;
    int is_verify = strcmp(a->command, "verify") == 0;
    int is_generate = strcmp(a->command, "generate") == 0;

    if (!is_up && !is_status && !is_verify && !is_generate)
        return usage_error("unknown command '%s'", a->command);

    int scratch_only = is_verify || is_generate; /* these never touch a real database */

    int i = 2;

    while (i < argc && strncmp(argv[i], "--", 2) == 0)
    {
        const char *opt = argv[i];

        if (strcmp(opt, "--allow-out-of-order") == 0)
        {
            if (scratch_only)
                return usage_error("--allow-out-of-order does not apply to %s", a->command);

            a->opts.allow_out_of_order = 1;
            i++;
            continue;
        }

        if (strcmp(opt, "--dry-run") == 0)
        {
            if (!is_generate)
                return usage_error("--dry-run only applies to generate%s", "");

            a->dry_run = 1;
            i++;
            continue;
        }

        int is_db = strcmp(opt, "--db") == 0;
        int is_dir = strcmp(opt, "--dir") == 0;
        int is_schema = strcmp(opt, "--schema") == 0;
        int is_name = strcmp(opt, "--name") == 0;
        int is_rename = strcmp(opt, "--rename") == 0;

        if (!is_db && !is_dir && !is_schema && !is_name && !is_rename)
            return usage_error("unknown option '%s'", opt);

        if (i + 1 >= argc)
            return usage_error("%s needs a value", opt);

        const char *value = argv[i + 1];

        if (is_db && scratch_only)
            return usage_error("%s uses a scratch database; --db does not apply", a->command);

        if (is_schema && !scratch_only)
            return usage_error("--schema only applies to verify and generate%s", "");

        if (is_name && !is_generate)
            return usage_error("--name only applies to generate%s", "");

        if (is_rename && !is_generate)
            return usage_error("--rename only applies to generate%s", "");

        if ((is_db && a->db) || (is_dir && a->dir) || (is_name && a->name))
            return usage_error("%s given more than once", opt);

        if (is_rename)
        {
            if (a->rename_count == BB_MIGRATE_RENAMES_MAX)
                return usage_error("too many --rename options (max %d)", "32");

            bb_migrate_err_t rename_err;

            if (bb_migrate_parse_rename(value, &a->renames[a->rename_count], &rename_err) != 0)
                return usage_error("%s", rename_err.msg);

            a->rename_count++;
        }
        else if (is_db)
            a->db = value;
        else if (is_dir)
            a->dir = value;
        else if (is_name)
            a->name = value;
        else
        {
            if (a->schema_count == MAX_SCHEMAS)
                return usage_error("too many --schema options (max %d)", "16");

            a->schemas[a->schema_count++] = value;
        }

        i += 2;
    }

    a->first_file = i;

    if ((is_up || is_status) && !a->db)
        return usage_error("%s needs --db <database>", a->command);

    if (scratch_only && a->schema_count == 0)
        return usage_error("%s needs at least one --schema <manifest>", a->command);

    if (is_generate)
    {
        if (!a->dir)
            return usage_error("generate needs --dir <directory> (where the new migration is written)%s", "");

        if (!a->name)
            return usage_error("generate needs --name <name> for the new migration%s", "");

        if (a->first_file < argc)
            return usage_error("generate reads migrations from --dir; unexpected argument '%s'", argv[a->first_file]);

        return 0;
    }

    if (!a->dir && a->first_file >= argc)
        return usage_error("no migrations given (use --dir or list files)%s", "");

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
                                 strcmp(args.command, "generate") == 0, /* allow an empty directory */
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
    else if (strcmp(args.command, "generate") == 0)
    {
        rc = cmd_generate(&args, &set);
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
