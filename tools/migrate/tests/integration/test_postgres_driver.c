/* bb-migrate against a real PostgreSQL server.
 *
 * Runs only when the build has BB_WITH_POSTGRES and BB_TEST_POSTGRES_URI points
 * at a database (a postgresql:// URI) where the user may create schemas.
 * Without them the test skips itself.
 *
 * Every test works inside its own uniquely named schema, which is dropped
 * again, so nothing in `public` or in other tests' tables is touched and the
 * database can be shared. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BB_WITH_POSTGRES

int main(void)
{
    printf("Skipping bb-migrate postgres tests (built without BB_WITH_POSTGRES)\n");
    return 0;
}

#else

#include "driver.h"
#include "drivers/postgres_driver.h"
#include "generate.h"
#include "migration.h"
#include "plan.h"
#include "runner.h"
#include "table_spec.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/uuid.h>

#include <libpq-fe.h>
#include <pthread.h>

static const char *g_uri;

/* ---------------------------
 * Helpers
 * --------------------------- */

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

/* Runs `sql` on a separate connection, independent of the driver under test,
 * and returns the first cell as a string (empty for no rows). */
static void admin_query(const char *sql, char *out, size_t cap)
{
    PGconn *c = PQconnectdb(g_uri);
    BB_ASSERT(PQstatus(c) == CONNECTION_OK);

    PQclear(PQexec(c, "SET client_min_messages TO warning")); /* no NOTICE noise from DROP ... CASCADE */

    PGresult *r = PQexec(c, sql);
    BB_ASSERT(PQresultStatus(r) == PGRES_TUPLES_OK || PQresultStatus(r) == PGRES_COMMAND_OK);

    snprintf(out, cap, "%s", PQntuples(r) > 0 && PQnfields(r) > 0 ? PQgetvalue(r, 0, 0) : "");

    PQclear(r);
    PQfinish(c);
}

static long long admin_count(const char *sql)
{
    char buf[64];
    admin_query(sql, buf, sizeof buf);
    return atoll(buf);
}

static void admin_exec(const char *sql)
{
    char ignore[8];
    admin_query(sql, ignore, sizeof ignore);
}

/* A private schema, plus a connection string whose search_path is that
 * schema, so the (non-scratch) driver puts its ledger and tables there. */
typedef struct {
    char name[64];
    char conninfo[512];
} sandbox_t;

static void sandbox_create(sandbox_t *s)
{
    bb_uuid_t id;
    BB_ASSERT(bb_uuid_v4_string(id) == 0);

    snprintf(s->name, sizeof s->name, "bb_mig_test_%.8s", id);

    char sql[128];
    snprintf(sql, sizeof sql, "CREATE SCHEMA %s", s->name);
    admin_exec(sql);

    if (strstr(g_uri, "://"))
        snprintf(s->conninfo, sizeof s->conninfo, "%s%coptions=-csearch_path%%3D%s",
                 g_uri, strchr(g_uri, '?') ? '&' : '?', s->name);
    else
        snprintf(s->conninfo, sizeof s->conninfo, "%s options='-c search_path=%s'", g_uri, s->name);
}

static void sandbox_drop(const sandbox_t *s)
{
    char sql[128];
    snprintf(sql, sizeof sql, "DROP SCHEMA %s CASCADE", s->name);
    admin_exec(sql);
}

static long long tables_named(const sandbox_t *s, const char *table)
{
    char sql[256];
    snprintf(sql, sizeof sql,
             "SELECT count(*) FROM information_schema.tables WHERE table_schema='%s' AND table_name='%s'",
             s->name, table);
    return admin_count(sql);
}

static long long ledger_rows(const sandbox_t *s)
{
    char sql[160];
    snprintf(sql, sizeof sql, "SELECT count(*) FROM %s.bb_schema_migrations", s->name);
    return admin_count(sql);
}

static bb_migrate_driver_t *open_sandboxed(const sandbox_t *s)
{
    bb_migrate_err_t err;
    bb_migrate_driver_t *d = bb_migrate_postgres_open(s->conninfo, &err);
    BB_ASSERT(d != NULL);
    return d;
}

typedef struct {
    int count;
} progress_t;

static void on_applied(const bb_migration_t *m, void *user)
{
    (void)m;
    ((progress_t *)user)->count++;
}

static void load(const char *const *paths, size_t n, bb_migration_set_t *set)
{
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_load_files(paths, n, set, &err) == 0);
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_dialect_and_connection_errors(void)
{
    printf("\tTesting the driver's dialect and connection errors...\n");

    bb_migrate_err_t err;

    /* a closed port, with a password that must never show up in the message */
    bb_migrate_driver_t *d = bb_migrate_postgres_open("postgresql://nobody:hunter2@127.0.0.1:1/none", &err);
    BB_ASSERT(d == NULL);
    BB_ASSERT(strstr(err.msg, "cannot connect to PostgreSQL") != NULL);
    BB_ASSERT(strstr(err.msg, "hunter2") == NULL);

    BB_ASSERT(bb_migrate_postgres_open("", &err) == NULL);
    BB_ASSERT(bb_migrate_postgres_open(NULL, &err) == NULL);

    d = bb_migrate_postgres_open(g_uri, &err);
    BB_ASSERT(d != NULL);
    BB_ASSERT(strcmp(d->api->name, "postgres") == 0);
    BB_ASSERT(d->api->dialect == BB_MIGRATE_DIALECT_POSTGRES);
    bb_migrate_driver_close(d);
}

static void test_status_is_read_only_and_up_applies(void)
{
    printf("\tTesting plan is read-only, up applies once and records it...\n");

    sandbox_t sb;
    sandbox_create(&sb);

    write_text_file("0001_pgmiga_create_users.sql",
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL);\n");
    write_text_file("0002_pgmiga_add_posts.sql",
        "CREATE TABLE posts (id INTEGER PRIMARY KEY, user_id INTEGER, body TEXT);\n"
        "CREATE INDEX posts_user ON posts (user_id);\n");

    const char *paths[] = { "0001_pgmiga_create_users.sql", "0002_pgmiga_add_posts.sql" };
    bb_migration_set_t set;
    load(paths, 2, &set);

    bb_migrate_driver_t *d = open_sandboxed(&sb);
    bb_migrate_err_t err;

    /* fresh database: planning reports both pending and creates nothing */
    bb_migrate_plan_t plan;
    BB_ASSERT(bb_migrate_plan_for(d, &set, NULL, &plan, &err) == 0);
    BB_ASSERT(plan.pending_count == 2 && plan.problem_count == 0);
    bb_migrate_plan_free(&plan);
    BB_ASSERT(tables_named(&sb, "bb_schema_migrations") == 0);

    progress_t progress = { 0 };
    size_t applied = 0;
    BB_ASSERT(bb_migrate_up(d, &set, NULL, on_applied, &progress, &applied, &err) == 0);
    BB_ASSERT(applied == 2 && progress.count == 2);

    BB_ASSERT(tables_named(&sb, "users") == 1);
    BB_ASSERT(tables_named(&sb, "posts") == 1);
    BB_ASSERT(ledger_rows(&sb) == 2);

    /* again: nothing to do */
    applied = 99;
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 0);
    BB_ASSERT(ledger_rows(&sb) == 2);

    BB_ASSERT(bb_migrate_plan_for(d, &set, NULL, &plan, &err) == 0);
    BB_ASSERT(plan.pending_count == 0 && plan.problem_count == 0);
    bb_migrate_plan_free(&plan);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
    sandbox_drop(&sb);
    remove(paths[0]);
    remove(paths[1]);
}

static void test_failed_migration_rolls_back_completely(void)
{
    printf("\tTesting a failing migration leaves no trace and stops the run...\n");

    sandbox_t sb;
    sandbox_create(&sb);

    write_text_file("0001_pgmigb_ok.sql", "CREATE TABLE ok (id INTEGER PRIMARY KEY);\n");
    write_text_file("0002_pgmigb_broken.sql",
        "CREATE TABLE half_done (id INTEGER PRIMARY KEY);\n"
        "INSERT INTO half_done VALUES (1);\n"
        "ALTER TABLE does_not_exist ADD COLUMN x INTEGER;\n");
    write_text_file("0003_pgmigb_after.sql", "CREATE TABLE after_broken (id INTEGER PRIMARY KEY);\n");

    const char *paths[] = { "0001_pgmigb_ok.sql", "0002_pgmigb_broken.sql", "0003_pgmigb_after.sql" };
    bb_migration_set_t set;
    load(paths, 3, &set);

    bb_migrate_driver_t *d = open_sandboxed(&sb);
    bb_migrate_err_t err;
    size_t applied = 0;

    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &applied, &err) == -1);
    BB_ASSERT(applied == 1);
    BB_ASSERT(strstr(err.msg, "migration 2") != NULL);
    BB_ASSERT(strstr(err.msg, "does_not_exist") != NULL);

    BB_ASSERT(tables_named(&sb, "ok") == 1);            /* earlier migration stays applied */
    BB_ASSERT(tables_named(&sb, "half_done") == 0);     /* PostgreSQL rolled the DDL back  */
    BB_ASSERT(tables_named(&sb, "after_broken") == 0);  /* and the run stopped             */
    BB_ASSERT(ledger_rows(&sb) == 1);

    /* fix the file and run again: continues where it stopped */
    bb_migration_set_free(&set);
    write_text_file("0002_pgmigb_broken.sql", "CREATE TABLE fixed (id INTEGER PRIMARY KEY);\n");
    load(paths, 3, &set);

    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 2);
    BB_ASSERT(tables_named(&sb, "fixed") == 1);
    BB_ASSERT(tables_named(&sb, "after_broken") == 1);
    BB_ASSERT(ledger_rows(&sb) == 3);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
    sandbox_drop(&sb);
    for (int i = 0; i < 3; i++)
        remove(paths[i]);
}

static void test_modified_and_missing_migrations_are_problems(void)
{
    printf("\tTesting an edited or missing applied migration blocks up...\n");

    sandbox_t sb;
    sandbox_create(&sb);

    write_text_file("0001_pgmigc_one.sql", "CREATE TABLE one (id INTEGER PRIMARY KEY);\n");
    write_text_file("0002_pgmigc_two.sql", "CREATE TABLE two (id INTEGER PRIMARY KEY);\n");

    const char *paths[] = { "0001_pgmigc_one.sql", "0002_pgmigc_two.sql" };
    bb_migration_set_t set;
    load(paths, 2, &set);

    bb_migrate_driver_t *d = open_sandboxed(&sb);
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, NULL, &err) == 0);
    bb_migration_set_free(&set);

    /* edit an applied migration */
    write_text_file("0001_pgmigc_one.sql", "CREATE TABLE one (id INTEGER PRIMARY KEY, sneaky TEXT);\n");
    load(paths, 2, &set);

    bb_migrate_plan_t plan;
    BB_ASSERT(bb_migrate_plan_for(d, &set, NULL, &plan, &err) == 0);
    BB_ASSERT(plan.problem_count == 1);
    BB_ASSERT(bb_migrate_plan_first_problem(&plan)->state == BB_MIGRATE_STATE_MODIFIED);
    bb_migrate_plan_free(&plan);
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, NULL, &err) == -1);
    bb_migration_set_free(&set);

    /* drop a file */
    const char *only_second[] = { "0002_pgmigc_two.sql" };
    load(only_second, 1, &set);
    BB_ASSERT(bb_migrate_plan_for(d, &set, NULL, &plan, &err) == 0);
    BB_ASSERT(plan.problem_count >= 1);
    {
        int saw_missing = 0;
        for (size_t i = 0; i < plan.count; i++)
            if (plan.entries[i].state == BB_MIGRATE_STATE_MISSING)
                saw_missing = 1;
        BB_ASSERT(saw_missing);
    }
    bb_migrate_plan_free(&plan);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
    sandbox_drop(&sb);
    remove(paths[0]);
    remove(paths[1]);
}

static void test_describe_table(void)
{
    printf("\tTesting describe_table reads types, primary keys and exact table names...\n");

    sandbox_t sb;
    sandbox_create(&sb);

    write_text_file("0001_pgmigd_tables.sql",
        "CREATE TABLE \"Mixed_Case\" (\n"
        "    \"id\" UUID PRIMARY KEY,\n"
        "    \"label\" TEXT,\n"
        "    \"n\" INTEGER,\n"
        "    \"data\" BYTEA,\n"
        "    \"code\" VARCHAR(20)\n"
        ");\n"
        "CREATE TABLE composite (a INTEGER, b INTEGER, note TEXT, PRIMARY KEY (a, b));\n");

    const char *paths[] = { "0001_pgmigd_tables.sql" };
    bb_migration_set_t set;
    load(paths, 1, &set);

    bb_migrate_driver_t *d = open_sandboxed(&sb);
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, NULL, &err) == 0);

    bb_migrate_table_spec_t spec;

    /* a case-sensitive name, as Persist's PostgreSQL backend creates them */
    BB_ASSERT(d->api->describe_table(d, "Mixed_Case", &spec) == 0);
    BB_ASSERT(spec.count == 5);
    BB_ASSERT(strcmp(spec.columns[0].name, "id") == 0 && strcmp(spec.columns[0].type, "UUID") == 0);
    BB_ASSERT(spec.columns[0].primary_key == 1);
    BB_ASSERT(strcmp(spec.columns[1].type, "TEXT") == 0 && spec.columns[1].primary_key == 0);
    BB_ASSERT(strcmp(spec.columns[2].type, "INTEGER") == 0);
    BB_ASSERT(strcmp(spec.columns[3].type, "BYTEA") == 0);
    BB_ASSERT(strcmp(spec.columns[4].type, "CHARACTER VARYING(20)") == 0); /* fits the type buffer */

    /* the lower-case spelling is a different (nonexistent) table */
    BB_ASSERT(d->api->describe_table(d, "mixed_case", &spec) == 0);
    BB_ASSERT(spec.count == 0);

    BB_ASSERT(d->api->describe_table(d, "composite", &spec) == 0);
    BB_ASSERT(spec.count == 3);
    BB_ASSERT(spec.columns[0].primary_key == 1 && spec.columns[1].primary_key == 1);
    BB_ASSERT(spec.columns[2].primary_key == 0);

    /* a name that is not an identifier is a bound parameter, not SQL */
    BB_ASSERT(d->api->describe_table(d, "x'; DROP SCHEMA public CASCADE; --", &spec) == 0);
    BB_ASSERT(spec.count == 0);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
    sandbox_drop(&sb);
    remove(paths[0]);
}

static void test_scratch_schema_is_isolated_and_dropped(void)
{
    printf("\tTesting scratch databases stay private and are dropped, even after a failure...\n");

    const char *leftover_sql =
        "SELECT count(*) FROM pg_namespace WHERE nspname LIKE 'bb_migrate_scratch_%'";
    long long before = admin_count(leftover_sql);

    write_text_file("0001_pgmige_probe.sql", "CREATE TABLE scratch_probe (id INTEGER PRIMARY KEY);\n");
    const char *paths[] = { "0001_pgmige_probe.sql" };
    bb_migration_set_t set;
    load(paths, 1, &set);

    bb_migrate_err_t err;
    bb_migrate_driver_t *s = bb_migrate_postgres_open_scratch(g_uri, &err);
    BB_ASSERT(s != NULL);

    BB_ASSERT(admin_count(leftover_sql) == before + 1);   /* its private schema exists */
    BB_ASSERT(bb_migrate_up(s, &set, NULL, NULL, NULL, NULL, &err) == 0);

    /* the table is in the scratch schema, and nowhere else */
    BB_ASSERT(admin_count("SELECT count(*) FROM information_schema.tables WHERE table_name='scratch_probe'") == 1);
    BB_ASSERT(admin_count("SELECT count(*) FROM information_schema.tables WHERE table_name='scratch_probe'"
                          " AND table_schema LIKE 'bb_migrate_scratch_%'") == 1);

    bb_migrate_driver_close(s);
    BB_ASSERT(admin_count(leftover_sql) == before);       /* dropped with everything in it */
    BB_ASSERT(admin_count("SELECT count(*) FROM information_schema.tables WHERE table_name='scratch_probe'") == 0);
    bb_migration_set_free(&set);

    /* a migration that fails inside the transaction must not stop the drop */
    write_text_file("0001_pgmige_probe.sql", "CREATE TABLE scratch_probe (id INTEGER);\nSELECT nope FROM nowhere;\n");
    load(paths, 1, &set);

    s = bb_migrate_postgres_open_scratch(g_uri, &err);
    BB_ASSERT(s != NULL);
    BB_ASSERT(bb_migrate_up(s, &set, NULL, NULL, NULL, NULL, &err) == -1);
    bb_migrate_driver_close(s);
    BB_ASSERT(admin_count(leftover_sql) == before);

    bb_migration_set_free(&set);
    remove(paths[0]);
}

static size_t check(bb_migrate_driver_t *d, const char *manifest)
{
    bb_migrate_table_spec_t expected;
    bb_migrate_err_t err;
    size_t diffs = 99;

    BB_ASSERT(bb_migrate_spec_from_manifest_for(d->api->dialect, manifest, &expected, &err) == 0);
    BB_ASSERT(bb_migrate_check_table(d, &expected, NULL, NULL, &diffs, &err) == 0);

    return diffs;
}

static void test_verify_and_generate_round_trip(void)
{
    printf("\tTesting verify finds drift and generate's SQL fixes it (PostgreSQL dialect)...\n");

    write_text_file("pgmigf.schema.json",
        "{ \"version\": 1, \"kind\": \"persist.schema\", \"name\": \"Task\", \"table\": \"tasks\","
        "  \"fields\": ["
        "    { \"name\": \"id\", \"type\": \"uuid\", \"primary_key\": true },"
        "    { \"name\": \"name\", \"type\": \"string\", \"size\": 64 },"
        "    { \"name\": \"priority\", \"type\": \"int\" },"
        "    { \"name\": \"payload\", \"type\": \"blob\", \"size\": 16 } ] }");

    write_text_file("0001_pgmigf_tasks.sql",
        "CREATE TABLE IF NOT EXISTS \"tasks\" (\"id\" UUID PRIMARY KEY, \"name\" TEXT);\n");

    const char *first[] = { "0001_pgmigf_tasks.sql" };
    bb_migration_set_t set;
    bb_migrate_err_t err;
    load(first, 1, &set);

    /* verify: the manifest wants two columns the migration doesn't create */
    bb_migrate_driver_t *s = bb_migrate_postgres_open_scratch(g_uri, &err);
    BB_ASSERT(s != NULL);
    BB_ASSERT(bb_migrate_up(s, &set, NULL, NULL, NULL, NULL, &err) == 0);
    BB_ASSERT(check(s, "pgmigf.schema.json") == 2);

    /* generate: diff the real table against the manifest, emit */
    bb_migrate_table_spec_t expected, actual;
    BB_ASSERT(bb_migrate_spec_from_manifest_for(BB_MIGRATE_DIALECT_POSTGRES, "pgmigf.schema.json", &expected, &err) == 0);
    BB_ASSERT(s->api->describe_table(s, "tasks", &actual) == 0);

    char *sql = NULL;
    BB_ASSERT(bb_migrate_generate_sql_for(BB_MIGRATE_DIALECT_POSTGRES, &expected, &actual, NULL, 0, &sql, &err)
              == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strstr(sql, "ALTER TABLE \"tasks\" ADD COLUMN \"priority\" INTEGER;") != NULL);
    BB_ASSERT(strstr(sql, "ALTER TABLE \"tasks\" ADD COLUMN \"payload\" BYTEA;") != NULL);
    bb_migrate_driver_close(s);
    bb_migration_set_free(&set);

    /* the generated migration, added to the set, makes verify pass */
    write_text_file("0002_pgmigf_more.sql", sql);
    free(sql);

    const char *both[] = { "0001_pgmigf_tasks.sql", "0002_pgmigf_more.sql" };
    load(both, 2, &set);

    s = bb_migrate_postgres_open_scratch(g_uri, &err);
    BB_ASSERT(s != NULL);
    BB_ASSERT(bb_migrate_up(s, &set, NULL, NULL, NULL, NULL, &err) == 0);
    BB_ASSERT(check(s, "pgmigf.schema.json") == 0);
    bb_migrate_driver_close(s);

    /* a missing table is generated as CREATE TABLE in Persist's shape */
    bb_migrate_table_spec_t none;
    memset(&none, 0, sizeof none);
    snprintf(none.table, sizeof none.table, "tasks");
    BB_ASSERT(bb_migrate_generate_sql_for(BB_MIGRATE_DIALECT_POSTGRES, &expected, &none, NULL, 0, &sql, &err)
              == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strstr(sql, "CREATE TABLE IF NOT EXISTS \"tasks\" (") != NULL);
    BB_ASSERT(strstr(sql, "\"id\" UUID PRIMARY KEY") != NULL);
    free(sql);

    bb_migration_set_free(&set);
    remove("pgmigf.schema.json");
    remove("0001_pgmigf_tasks.sql");
    remove("0002_pgmigf_more.sql");
}

/* ---------------------------
 * Concurrent runners
 * --------------------------- */

typedef struct {
    const sandbox_t *sb;
    const bb_migration_set_t *set;
    int rc;
    size_t applied;
    char msg[BB_MIGRATE_ERR_MAX];
} runner_arg_t;

static void *runner_thread(void *p)
{
    runner_arg_t *a = p;
    bb_migrate_err_t err;

    bb_migrate_driver_t *d = bb_migrate_postgres_open(a->sb->conninfo, &err);
    BB_ASSERT(d != NULL);

    a->rc = bb_migrate_up(d, a->set, NULL, NULL, NULL, &a->applied, &err);
    snprintf(a->msg, sizeof a->msg, "%s", a->rc == 0 ? "" : err.msg);

    bb_migrate_driver_close(d);
    return NULL;
}

static void test_concurrent_runners_stay_consistent(void)
{
    printf("\tTesting two simultaneous `up` runs leave a consistent database...\n");

    sandbox_t sb;
    sandbox_create(&sb);

    write_text_file("0001_pgmigg_a.sql", "CREATE TABLE ga (id INTEGER PRIMARY KEY);\n");
    write_text_file("0002_pgmigg_b.sql", "CREATE TABLE gb (id INTEGER PRIMARY KEY);\n");
    write_text_file("0003_pgmigg_c.sql", "CREATE TABLE gc (id INTEGER PRIMARY KEY);\n");

    const char *paths[] = { "0001_pgmigg_a.sql", "0002_pgmigg_b.sql", "0003_pgmigg_c.sql" };
    bb_migration_set_t set;
    load(paths, 3, &set);

    runner_arg_t a = { &sb, &set, -1, 0, "" }, b = { &sb, &set, -1, 0, "" };
    pthread_t ta, tb;
    BB_ASSERT(pthread_create(&ta, NULL, runner_thread, &a) == 0);
    BB_ASSERT(pthread_create(&tb, NULL, runner_thread, &b) == 0);
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    /* however they interleaved: every migration ran exactly once... */
    BB_ASSERT(ledger_rows(&sb) == 3);
    BB_ASSERT(tables_named(&sb, "ga") == 1);
    BB_ASSERT(tables_named(&sb, "gb") == 1);
    BB_ASSERT(tables_named(&sb, "gc") == 1);

    /* ...at least one runner got through, and a loser says why */
    BB_ASSERT(a.rc == 0 || b.rc == 0);
    BB_ASSERT(a.applied + b.applied == 3);
    if (a.rc != 0)
        BB_ASSERT(strstr(a.msg, "migration 1") != NULL || strstr(a.msg, "migration 2") != NULL ||
                  strstr(a.msg, "migration 3") != NULL);
    if (b.rc != 0)
        BB_ASSERT(strstr(b.msg, "migration 1") != NULL || strstr(b.msg, "migration 2") != NULL ||
                  strstr(b.msg, "migration 3") != NULL);

    /* the lock is gone with the runners: a third run is a quick no-op */
    bb_migrate_driver_t *d = open_sandboxed(&sb);
    bb_migrate_err_t err;
    size_t applied = 99;
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 0);
    bb_migrate_driver_close(d);

    bb_migration_set_free(&set);
    sandbox_drop(&sb);
    for (int i = 0; i < 3; i++)
        remove(paths[i]);
}

/* Two runners starting on a database that has no ledger yet both try to create
 * it. CREATE TABLE IF NOT EXISTS alone is not safe against that (the loser can
 * fail on a catalog duplicate), so the driver serialises ledger creation.
 * One attempt rarely collides, so repeat it on fresh databases. */
static void test_concurrent_first_run_creates_the_ledger_once(void)
{
    printf("\tTesting simultaneous first runs never fail while creating the ledger...\n");

    write_text_file("0001_pgmigh_only.sql", "CREATE TABLE only_one (id INTEGER PRIMARY KEY);\n");
    const char *paths[] = { "0001_pgmigh_only.sql" };
    bb_migration_set_t set;
    load(paths, 1, &set);

    for (int round = 0; round < 25; round++)
    {
        sandbox_t sb;
        sandbox_create(&sb);

        runner_arg_t a = { &sb, &set, -1, 0, "" }, b = { &sb, &set, -1, 0, "" };
        pthread_t ta, tb;
        BB_ASSERT(pthread_create(&ta, NULL, runner_thread, &a) == 0);
        BB_ASSERT(pthread_create(&tb, NULL, runner_thread, &b) == 0);
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);

        if ((a.rc != 0 && !strstr(a.msg, "migration 1")) || (b.rc != 0 && !strstr(b.msg, "migration 1")))
            fprintf(stderr, "\t  round %d: runner A rc=%d %s\n\t           runner B rc=%d %s\n",
                    round, a.rc, a.msg, b.rc, b.msg);

        /* the migration ran once; a runner that lost reports the ledger's
         * duplicate key for migration 1, never a failure to create the ledger */
        BB_ASSERT(a.rc == 0 || b.rc == 0);
        BB_ASSERT(a.rc == 0 || strstr(a.msg, "migration 1") != NULL);
        BB_ASSERT(b.rc == 0 || strstr(b.msg, "migration 1") != NULL);
        BB_ASSERT(a.applied + b.applied == 1);
        BB_ASSERT(ledger_rows(&sb) == 1);
        BB_ASSERT(tables_named(&sb, "only_one") == 1);

        sandbox_drop(&sb);
    }

    bb_migration_set_free(&set);
    remove(paths[0]);
}

int main(void)
{
    g_uri = getenv("BB_TEST_POSTGRES_URI");

    if (!g_uri || !*g_uri)
    {
        printf("Skipping bb-migrate postgres tests (BB_TEST_POSTGRES_URI not set)\n");
        return 0;
    }

    printf("Running bb-migrate postgres tests...\n");

    test_dialect_and_connection_errors();
    test_status_is_read_only_and_up_applies();
    test_failed_migration_rolls_back_completely();
    test_modified_and_missing_migrations_are_problems();
    test_describe_table();
    test_scratch_schema_is_isolated_and_dropped();
    test_verify_and_generate_round_trip();
    test_concurrent_runners_stay_consistent();
    test_concurrent_first_run_creates_the_ledger_once();

    printf("All bb-migrate postgres tests passed\n");
    return 0;
}

#endif /* BB_WITH_POSTGRES */
