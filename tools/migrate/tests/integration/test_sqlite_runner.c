#include "driver.h"
#include "drivers/sqlite_driver.h"
#include "migration.h"
#include "plan.h"
#include "runner.h"

#include <blue-bird/error/assert.h>

#include <sqlite3.h>

#include <stdio.h>
#include <string.h>

#define DB_PATH "test_migrate_runner.db"

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

/* Runs a scalar "SELECT COUNT(*)..." against the database file directly,
 * independent of the driver under test. */
static long long scalar(const char *sql)
{
    sqlite3 *db = NULL;
    BB_ASSERT(sqlite3_open(DB_PATH, &db) == SQLITE_OK);

    sqlite3_stmt *st = NULL;
    BB_ASSERT(sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK);
    BB_ASSERT(sqlite3_step(st) == SQLITE_ROW);
    long long v = sqlite3_column_int64(st, 0);

    sqlite3_finalize(st);
    sqlite3_close(db);
    return v;
}

static long long table_exists(const char *name)
{
    char sql[256];
    snprintf(sql, sizeof sql,
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='%s'", name);
    return scalar(sql);
}

static long long ledger_rows(void)
{
    return scalar("SELECT COUNT(*) FROM bb_schema_migrations");
}

static void cleanup_files(void)
{
    remove(DB_PATH);
    remove("0001_create_users.sql");
    remove("0002_add_posts.sql");
    remove("0003_seed_admin.sql");
    remove("0004_broken.sql");
    remove("0005_after_broken.sql");
}

static void write_base_migrations(void)
{
    write_text_file("0001_create_users.sql",
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL);\n");
    write_text_file("0002_add_posts.sql",
        "CREATE TABLE posts (id INTEGER PRIMARY KEY, user_id INTEGER, body TEXT);\n"
        "CREATE INDEX posts_user ON posts(user_id);\n");
}

static void load(bb_migration_set_t *set, const char *const *paths, size_t n)
{
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_load_files(paths, n, set, &err) == 0);
}

static bb_migrate_driver_t *open_driver(void)
{
    bb_migrate_err_t err;
    bb_migrate_driver_t *d = bb_migrate_sqlite_open(DB_PATH, &err);
    BB_ASSERT(d != NULL);
    return d;
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_status_is_read_only(void)
{
    printf("\tTesting status on a fresh database creates nothing...\n");

    cleanup_files();
    write_base_migrations();

    const char *paths[] = { "0001_create_users.sql", "0002_add_posts.sql" };
    bb_migration_set_t set;
    load(&set, paths, 2);

    bb_migrate_driver_t *d = open_driver();
    bb_migrate_plan_t plan;
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_plan_for(d, &set, &plan, &err) == 0);
    BB_ASSERT(plan.pending_count == 2 && plan.problem_count == 0);
    bb_migrate_plan_free(&plan);
    bb_migrate_driver_close(d);

    BB_ASSERT(table_exists("bb_schema_migrations") == 0);
    BB_ASSERT(table_exists("users") == 0);

    bb_migration_set_free(&set);
}

static void test_up_applies_and_is_idempotent(void)
{
    printf("\tTesting up applies in order, then is a no-op...\n");

    const char *paths[] = { "0002_add_posts.sql", "0001_create_users.sql" }; /* shuffled */
    bb_migration_set_t set;
    load(&set, paths, 2);

    bb_migrate_driver_t *d = open_driver();
    bb_migrate_err_t err;
    size_t applied = 99;

    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 2);
    BB_ASSERT(table_exists("users") == 1);
    BB_ASSERT(table_exists("posts") == 1);
    BB_ASSERT(ledger_rows() == 2);

    /* posts references nothing in users, but 0002 must not have run
     * before 0001 recorded: versions come back ascending. */
    BB_ASSERT(scalar("SELECT MIN(version) FROM bb_schema_migrations") == 1);
    BB_ASSERT(scalar("SELECT MAX(version) FROM bb_schema_migrations") == 2);

    /* Second run, same driver. */
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 0);
    bb_migrate_driver_close(d);

    /* Third run, brand-new connection: state lives in the database. */
    d = open_driver();
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 0);
    BB_ASSERT(ledger_rows() == 2);
    bb_migrate_driver_close(d);

    bb_migration_set_free(&set);
}

static int g_progress_calls = 0;

static void count_progress(const bb_migration_t *m, void *user)
{
    (void)m;
    (*(int *)user)++;
    g_progress_calls++;
}

static void test_version_upgrade_applies_only_new(void)
{
    printf("\tTesting a new release applies only its new migration...\n");

    write_text_file("0003_seed_admin.sql",
        "INSERT INTO users (id, name) VALUES (1, 'admin');\n");

    const char *paths[] = { "0001_create_users.sql", "0002_add_posts.sql", "0003_seed_admin.sql" };
    bb_migration_set_t set;
    load(&set, paths, 3);

    bb_migrate_driver_t *d = open_driver();
    bb_migrate_err_t err;
    size_t applied = 0;
    int progress = 0;

    BB_ASSERT(bb_migrate_up(d, &set, count_progress, &progress, &applied, &err) == 0);
    BB_ASSERT(applied == 1);
    BB_ASSERT(progress == 1);
    BB_ASSERT(scalar("SELECT COUNT(*) FROM users") == 1);
    BB_ASSERT(ledger_rows() == 3);

    /* Ledger rows carry what we expect. */
    BB_ASSERT(scalar("SELECT COUNT(*) FROM bb_schema_migrations "
                     "WHERE version = 3 AND name = 'seed_admin' AND length(checksum) = 64 "
                     "AND applied_at > 0") == 1);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
}

static void test_failed_migration_rolls_back(void)
{
    printf("\tTesting a failing migration rolls back and stops the run...\n");

    /* First statement is valid and would create a table; second fails.
     * Neither may survive. */
    write_text_file("0004_broken.sql",
        "CREATE TABLE half_done (id INTEGER);\n"
        "INSERT INTO table_that_does_not_exist VALUES (1);\n");
    write_text_file("0005_after_broken.sql",
        "CREATE TABLE should_not_exist (id INTEGER);\n");

    const char *paths[] = {
        "0001_create_users.sql", "0002_add_posts.sql", "0003_seed_admin.sql",
        "0004_broken.sql", "0005_after_broken.sql"
    };
    bb_migration_set_t set;
    load(&set, paths, 5);

    bb_migrate_driver_t *d = open_driver();
    bb_migrate_err_t err;
    size_t applied = 99;

    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, &applied, &err) != 0);
    BB_ASSERT(applied == 0);
    BB_ASSERT(strstr(err.msg, "migration 4 (broken) failed") != NULL);
    BB_ASSERT(strstr(err.msg, "table_that_does_not_exist") != NULL);

    BB_ASSERT(table_exists("half_done") == 0);       /* statement 1 rolled back */
    BB_ASSERT(table_exists("should_not_exist") == 0); /* run stopped */
    BB_ASSERT(ledger_rows() == 3);                    /* earlier work intact, 4 not recorded */

    /* The connection is still usable after the rollback. */
    BB_ASSERT(d->api->ensure_ledger(d) == 0);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
}

static void test_fix_and_rerun_recovers(void)
{
    printf("\tTesting fixing the broken file and re-running...\n");

    write_text_file("0004_broken.sql", "CREATE TABLE half_done (id INTEGER);\n");

    const char *paths[] = {
        "0001_create_users.sql", "0002_add_posts.sql", "0003_seed_admin.sql",
        "0004_broken.sql", "0005_after_broken.sql"
    };
    bb_migration_set_t set;
    load(&set, paths, 5);

    bb_migrate_driver_t *d = open_driver();
    bb_migrate_err_t err;
    size_t applied = 0;

    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 2);
    BB_ASSERT(table_exists("half_done") == 1);
    BB_ASSERT(table_exists("should_not_exist") == 1);
    BB_ASSERT(ledger_rows() == 5);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
}

static void test_modified_migration_blocks_up(void)
{
    printf("\tTesting an edited, already-applied migration blocks up...\n");

    write_text_file("0001_create_users.sql",
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, extra TEXT);\n");
    write_text_file("0006_new.sql", "CREATE TABLE brand_new (id INTEGER);\n");

    const char *paths[] = {
        "0001_create_users.sql", "0002_add_posts.sql", "0003_seed_admin.sql",
        "0004_broken.sql", "0005_after_broken.sql", "0006_new.sql"
    };
    bb_migration_set_t set;
    load(&set, paths, 6);

    bb_migrate_driver_t *d = open_driver();
    bb_migrate_err_t err;
    size_t applied = 99;

    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, &applied, &err) != 0);
    BB_ASSERT(applied == 0);
    BB_ASSERT(strstr(err.msg, "refusing to migrate") != NULL);
    BB_ASSERT(strstr(err.msg, "migration 1 (create_users) was modified") != NULL);

    /* Nothing changed, including the pending 0006. */
    BB_ASSERT(table_exists("brand_new") == 0);
    BB_ASSERT(ledger_rows() == 5);

    bb_migrate_plan_t plan;
    BB_ASSERT(bb_migrate_plan_for(d, &set, &plan, &err) == 0);
    BB_ASSERT(plan.entries[0].state == BB_MIGRATE_STATE_MODIFIED);
    BB_ASSERT(plan.problem_count == 1 && plan.pending_count == 1);
    bb_migrate_plan_free(&plan);

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
    remove("0006_new.sql");
}

static void test_missing_file_blocks_up(void)
{
    printf("\tTesting an applied migration with no file blocks up...\n");

    /* Only 0001 supplied although 5 are applied. */
    write_text_file("0001_create_users.sql",
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL);\n");

    const char *paths[] = { "0001_create_users.sql" };
    bb_migration_set_t set;
    load(&set, paths, 1);

    bb_migrate_driver_t *d = open_driver();
    bb_migrate_err_t err;
    size_t applied = 99;

    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, &applied, &err) != 0);
    BB_ASSERT(applied == 0);
    BB_ASSERT(strstr(err.msg, "file was not provided") != NULL);
    BB_ASSERT(strstr(err.msg, "more problem") != NULL); /* 2..5 are missing too */

    bb_migrate_driver_close(d);
    bb_migration_set_free(&set);
}

static void test_open_failure(void)
{
    printf("\tTesting open failure reports an error...\n");

    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_sqlite_open("/nonexistent_dir_for_bb_migrate/x.db", &err) == NULL);
    BB_ASSERT(strstr(err.msg, "cannot open database") != NULL);
    BB_ASSERT(bb_migrate_sqlite_open(NULL, &err) == NULL);
}

int main(void)
{
    printf("Running migration SQLite runner integration tests...\n");

    /* Order matters: each test builds on the database left by the last,
     * which is exactly the "database over successive releases" story. */
    test_status_is_read_only();
    test_up_applies_and_is_idempotent();
    test_version_upgrade_applies_only_new();
    test_failed_migration_rolls_back();
    test_fix_and_rerun_recovers();
    test_modified_migration_blocks_up();
    test_missing_file_blocks_up();
    test_open_failure();

    cleanup_files();
    remove("0006_new.sql");

    printf("All migration SQLite runner tests passed (%d progress callbacks).\n", g_progress_calls);
    return 0;
}
