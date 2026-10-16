#include "drivers/sqlite_driver.h"
#include "generate.h"
#include "migration.h"
#include "runner.h"
#include "table_spec.h"

#include <sqlite3.h>

#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The properties that matter, against a real SQLite:
 *   1. generated SQL runs, and afterwards the table matches the manifest
 *   2. generated migrations stack: release after release, no drift
 *   3. the generated first migration builds the table Persist would
 *   4. refused changes produce no SQL at all */

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static const char *V1 =
    "{ \"kind\": \"persist.schema\", \"name\": \"Task\", \"table\": \"tasks\", \"fields\": ["
    "  { \"name\": \"id\", \"type\": \"uuid\", \"primary_key\": true },"
    "  { \"name\": \"name\", \"type\": \"string\", \"size\": 64 } ] }";

static const char *V2_ADDS_COLUMNS =
    "{ \"kind\": \"persist.schema\", \"name\": \"Task\", \"table\": \"tasks\", \"fields\": ["
    "  { \"name\": \"id\", \"type\": \"uuid\", \"primary_key\": true },"
    "  { \"name\": \"name\", \"type\": \"string\", \"size\": 64 },"
    "  { \"name\": \"due\", \"type\": \"int\" },"
    "  { \"name\": \"payload\", \"type\": \"blob\", \"size\": 32 } ] }";

static const char *V3_RENAMES_COLUMN =
    "{ \"kind\": \"persist.schema\", \"name\": \"Task\", \"table\": \"tasks\", \"fields\": ["
    "  { \"name\": \"id\", \"type\": \"uuid\", \"primary_key\": true },"
    "  { \"name\": \"title\", \"type\": \"string\", \"size\": 64 },"
    "  { \"name\": \"due\", \"type\": \"int\" },"
    "  { \"name\": \"payload\", \"type\": \"blob\", \"size\": 32 } ] }";

/* What `generate` does, minus the file I/O: replay migrations on a scratch
 * database, diff against the manifest, generate SQL. Returns the generator's
 * result code; *sql_out is the SQL (caller frees) on BB_MIGRATE_GEN_OK. */
static int generate_for(const char *const *migration_files, size_t n,
                        const char *manifest,
                        const bb_migrate_rename_t *renames, size_t rename_count,
                        char **sql_out, bb_migrate_err_t *err,
                        bb_migrate_driver_t **scratch_out)
{
    bb_migration_set_t set;
    BB_ASSERT(bb_migrate_load_files(migration_files, n, &set, err) == 0);

    bb_migrate_driver_t *scratch = bb_migrate_sqlite_open(":memory:", err);
    BB_ASSERT(scratch != NULL);
    BB_ASSERT(bb_migrate_up(scratch, &set, NULL, NULL, NULL, NULL, err) == 0);
    bb_migration_set_free(&set);

    bb_migrate_table_spec_t expected, actual;
    BB_ASSERT(bb_migrate_spec_from_manifest(manifest, &expected, err) == 0);
    BB_ASSERT(scratch->api->describe_table(scratch, expected.table, &actual) == 0);

    int rc = bb_migrate_generate_sql(&expected, &actual, renames, rename_count, sql_out, err);

    if (scratch_out)
        *scratch_out = scratch;
    else
        bb_migrate_driver_close(scratch);

    return rc;
}

static size_t diffs_after(bb_migrate_driver_t *scratch, const char *manifest)
{
    bb_migrate_table_spec_t expected;
    bb_migrate_err_t err;
    size_t diffs = 99;

    BB_ASSERT(bb_migrate_spec_from_manifest(manifest, &expected, &err) == 0);
    BB_ASSERT(bb_migrate_check_table(scratch, &expected, NULL, NULL, &diffs, &err) == 0);
    return diffs;
}

static void test_lifecycle(void)
{
    printf("\tTesting generate -> apply -> generate -> apply leaves no drift...\n");

    write_text_file("gen_v1.json", V1);
    write_text_file("gen_v2.json", V2_ADDS_COLUMNS);

    bb_migrate_err_t err;
    char *sql = NULL;
    bb_migrate_driver_t *scratch = NULL;

    /* Release 1: no migrations at all. The manifest alone yields 0001. */
    BB_ASSERT(generate_for(NULL, 0, "gen_v1.json", NULL, 0, &sql, &err, &scratch) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strstr(sql, "CREATE TABLE IF NOT EXISTS tasks") != NULL);
    BB_ASSERT(scratch->api->exec(scratch, sql) == 0);
    BB_ASSERT(diffs_after(scratch, "gen_v1.json") == 0);
    bb_migrate_driver_close(scratch);

    write_text_file("0001_create_tasks.sql", sql);
    free(sql);

    /* Nothing changed: nothing to generate. */
    const char *m1[] = { "0001_create_tasks.sql" };
    BB_ASSERT(generate_for(m1, 1, "gen_v1.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_NOTHING);

    /* Release 2: the manifest gains two columns. */
    BB_ASSERT(generate_for(m1, 1, "gen_v2.json", NULL, 0, &sql, &err, &scratch) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strstr(sql, "ALTER TABLE tasks ADD COLUMN due INTEGER;") != NULL);
    BB_ASSERT(strstr(sql, "ALTER TABLE tasks ADD COLUMN payload BLOB;") != NULL);
    BB_ASSERT(strstr(sql, "CREATE TABLE") == NULL);
    BB_ASSERT(scratch->api->exec(scratch, sql) == 0);
    BB_ASSERT(diffs_after(scratch, "gen_v2.json") == 0);
    bb_migrate_driver_close(scratch);

    write_text_file("0002_add_due_and_payload.sql", sql);
    free(sql);

    /* Both generated files together are exactly what verify wants. */
    const char *m2[] = { "0001_create_tasks.sql", "0002_add_due_and_payload.sql" };
    BB_ASSERT(generate_for(m2, 2, "gen_v2.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_NOTHING);

    remove("0001_create_tasks.sql");
    remove("0002_add_due_and_payload.sql");
    remove("gen_v1.json");
    remove("gen_v2.json");
}

static void test_generated_first_migration_matches_persist_shape(void)
{
    printf("\tTesting 0001 builds the same table Persist's backend would...\n");

    write_text_file("gen_v1.json", V1);

    bb_migrate_err_t err;
    char *sql = NULL;
    bb_migrate_driver_t *scratch = NULL;

    BB_ASSERT(generate_for(NULL, 0, "gen_v1.json", NULL, 0, &sql, &err, &scratch) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(scratch->api->exec(scratch, sql) == 0);

    /* Persist emits: CREATE TABLE IF NOT EXISTS tasks (id TEXT PRIMARY KEY, name TEXT);
     * build that independently and compare shapes. */
    bb_migrate_driver_t *persist_like = bb_migrate_sqlite_open(":memory:", &err);
    BB_ASSERT(persist_like != NULL);
    BB_ASSERT(persist_like->api->exec(persist_like,
        "CREATE TABLE IF NOT EXISTS tasks (id TEXT PRIMARY KEY, name TEXT);") == 0);

    bb_migrate_table_spec_t a, b;
    BB_ASSERT(scratch->api->describe_table(scratch, "tasks", &a) == 0);
    BB_ASSERT(persist_like->api->describe_table(persist_like, "tasks", &b) == 0);
    BB_ASSERT(bb_migrate_spec_compare(&a, &b, NULL, NULL) == 0);

    /* ...and re-running it on a database Persist already created is harmless. */
    BB_ASSERT(persist_like->api->exec(persist_like, sql) == 0);

    bb_migrate_driver_close(persist_like);
    bb_migrate_driver_close(scratch);
    free(sql);
    remove("gen_v1.json");
}

static void test_rename_is_refused(void)
{
    printf("\tTesting a renamed column is refused and yields no SQL...\n");

    write_text_file("gen_v1.json", V1);
    write_text_file("gen_v2.json", V2_ADDS_COLUMNS);
    write_text_file("gen_v3.json", V3_RENAMES_COLUMN);

    bb_migrate_err_t err;
    char *sql = NULL;

    /* Build up the 0001 + 0002 history for real. */
    BB_ASSERT(generate_for(NULL, 0, "gen_v1.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_OK);
    write_text_file("0001_create_tasks.sql", sql);
    free(sql);

    const char *m1[] = { "0001_create_tasks.sql" };
    BB_ASSERT(generate_for(m1, 1, "gen_v2.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_OK);
    write_text_file("0002_add_more.sql", sql);
    free(sql);

    /* name -> title: "name" is only in the database, "title" only in the schema. */
    const char *m2[] = { "0001_create_tasks.sql", "0002_add_more.sql" };
    BB_ASSERT(generate_for(m2, 2, "gen_v3.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);
    BB_ASSERT(strstr(err.msg, "'name'") != NULL);
    BB_ASSERT(strstr(err.msg, "write this migration by hand") != NULL);
    BB_ASSERT(strstr(err.msg, "ADD COLUMN title") == NULL); /* no half-answer */

    remove("0001_create_tasks.sql");
    remove("0002_add_more.sql");
    remove("gen_v1.json");
    remove("gen_v2.json");
    remove("gen_v3.json");
}


static int raw_exec(const char *db_path, const char *sql)
{
    sqlite3 *db = NULL;
    BB_ASSERT(sqlite3_open(db_path, &db) == SQLITE_OK);
    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    sqlite3_close(db);
    return rc;
}

/* Reads one text value with a raw SQLite connection, independent of the tool. */
static void raw_text(const char *db_path, const char *sql, char *out, size_t cap)
{
    sqlite3 *db = NULL;
    BB_ASSERT(sqlite3_open(db_path, &db) == SQLITE_OK);

    sqlite3_stmt *st = NULL;
    BB_ASSERT(sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK);
    BB_ASSERT(sqlite3_step(st) == SQLITE_ROW);

    const unsigned char *t = sqlite3_column_text(st, 0);
    snprintf(out, cap, "%s", t ? (const char *)t : "");

    sqlite3_finalize(st);
    sqlite3_close(db);
}

static void test_rename_preserves_data(void)
{
    printf("\tTesting a generated rename keeps the column's data (unlike drop + add)...\n");

    const char *db_path = "test_gen_rename.db";
    remove(db_path);

    write_text_file("gen_v1.json", V1);
    write_text_file("gen_v3.json", V3_RENAMES_COLUMN); /* name -> title, plus due and payload */

    bb_migrate_err_t err;
    char *sql = NULL;

    /* 0001, generated from the first manifest, applied to a real file. */
    BB_ASSERT(generate_for(NULL, 0, "gen_v1.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_OK);
    write_text_file("0001_create_tasks.sql", sql);
    free(sql);

    const char *m1[] = { "0001_create_tasks.sql" };
    bb_migration_set_t set;
    BB_ASSERT(bb_migrate_load_files(m1, 1, &set, &err) == 0);

    bb_migrate_driver_t *d = bb_migrate_sqlite_open(db_path, &err);
    BB_ASSERT(d != NULL);
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, NULL, &err) == 0);
    bb_migration_set_free(&set);

    /* Real data exists before the schema changes. */
    BB_ASSERT(raw_exec(db_path, "INSERT INTO tasks (id, name) VALUES ('u1', 'buy milk')") == SQLITE_OK);

    /* Without the hint the rename is refused, with a precise suggestion. */
    BB_ASSERT(generate_for(m1, 1, "gen_v3.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "add --rename tasks.name=title") != NULL);

    /* With it: a rename, plus the two additions. */
    bb_migrate_rename_t hint;
    memset(&hint, 0, sizeof hint);
    snprintf(hint.table, sizeof hint.table, "tasks");
    snprintf(hint.from, sizeof hint.from, "name");
    snprintf(hint.to, sizeof hint.to, "title");

    bb_migrate_driver_t *scratch = NULL;
    BB_ASSERT(generate_for(m1, 1, "gen_v3.json", &hint, 1, &sql, &err, &scratch) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strstr(sql, "ALTER TABLE tasks RENAME COLUMN name TO title;") != NULL);
    BB_ASSERT(strstr(sql, "ALTER TABLE tasks ADD COLUMN due INTEGER;") != NULL);
    BB_ASSERT(strstr(sql, "DROP") == NULL);
    BB_ASSERT(scratch->api->exec(scratch, sql) == 0);
    BB_ASSERT(diffs_after(scratch, "gen_v3.json") == 0);
    bb_migrate_driver_close(scratch);

    write_text_file("0002_rename_name_to_title.sql", sql);
    free(sql);

    /* Apply it to the real database that holds the row. */
    const char *m2[] = { "0001_create_tasks.sql", "0002_rename_name_to_title.sql" };
    BB_ASSERT(bb_migrate_load_files(m2, 2, &set, &err) == 0);

    size_t applied = 0;
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &applied, &err) == 0);
    BB_ASSERT(applied == 1);
    bb_migration_set_free(&set);
    bb_migrate_driver_close(d);

    char value[64];
    raw_text(db_path, "SELECT title FROM tasks WHERE id = 'u1'", value, sizeof value);
    BB_ASSERT(strcmp(value, "buy milk") == 0); /* the data survived the rename */

    raw_text(db_path,
        "SELECT COUNT(*) FROM pragma_table_info('tasks') WHERE name = 'name'", value, sizeof value);
    BB_ASSERT(strcmp(value, "0") == 0);

    /* And the history now reproduces the schema exactly. */
    BB_ASSERT(generate_for(m2, 2, "gen_v3.json", NULL, 0, &sql, &err, NULL) == BB_MIGRATE_GEN_NOTHING);

    remove(db_path);
    remove("0001_create_tasks.sql");
    remove("0002_rename_name_to_title.sql");
    remove("gen_v1.json");
    remove("gen_v3.json");
}

int main(void)
{
    printf("Running migration SQLite generate integration tests...\n");

    test_lifecycle();
    test_generated_first_migration_matches_persist_shape();
    test_rename_is_refused();
    test_rename_preserves_data();

    printf("All migration SQLite generate tests passed!\n");
    return 0;
}
