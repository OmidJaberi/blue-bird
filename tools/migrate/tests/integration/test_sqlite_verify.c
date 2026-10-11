#include "driver.h"
#include "drivers/sqlite_driver.h"
#include "migration.h"
#include "runner.h"
#include "table_spec.h"

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

typedef struct {
    char text[2048];
    int count;
} collector_t;

static void collect(const char *message, void *user)
{
    collector_t *c = user;
    strncat(c->text, message, sizeof c->text - strlen(c->text) - 1);
    strncat(c->text, "\n", sizeof c->text - strlen(c->text) - 1);
    c->count++;
}

static bb_migrate_driver_t *scratch(void)
{
    bb_migrate_err_t err;
    bb_migrate_driver_t *d = bb_migrate_sqlite_open(":memory:", &err);
    BB_ASSERT(d != NULL);
    return d;
}

/* Applies the given migration files to a fresh scratch database. */
static bb_migrate_driver_t *scratch_with(const char *const *paths, size_t n)
{
    bb_migration_set_t set;
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_load_files(paths, n, &set, &err) == 0);

    bb_migrate_driver_t *d = scratch();
    BB_ASSERT(bb_migrate_up(d, &set, NULL, NULL, NULL, &err) == 0);

    bb_migration_set_free(&set);
    return d;
}

static size_t check(bb_migrate_driver_t *d, const char *manifest_path, collector_t *c)
{
    bb_migrate_table_spec_t expected;
    bb_migrate_err_t err;
    size_t diffs = 99;

    BB_ASSERT(bb_migrate_spec_from_manifest(manifest_path, &expected, &err) == 0);
    BB_ASSERT(bb_migrate_check_table(d, &expected, collect, c, &diffs, &err) == 0);
    return diffs;
}

#define TASK_MANIFEST(extra_field) \
    "{ \"version\": 1, \"kind\": \"persist.schema\", \"name\": \"Task\", \"table\": \"tasks\"," \
    "  \"fields\": [" \
    "    { \"name\": \"id\", \"type\": \"uuid\", \"primary_key\": true }," \
    "    { \"name\": \"name\", \"type\": \"string\", \"size\": 64 }," \
    "    { \"name\": \"status\", \"type\": \"string\", \"size\": 64 }" extra_field " ] }"

static void test_describe_table(void)
{
    printf("\tTesting describe_table (types upper-cased, pk flagged, missing table = empty)...\n");

    bb_migrate_driver_t *d = scratch();
    BB_ASSERT(d->api->exec(d, "create table t (id text primary key, n integer, b blob)") == 0);

    bb_migrate_table_spec_t spec;
    BB_ASSERT(d->api->describe_table(d, "t", &spec) == 0);
    BB_ASSERT(spec.count == 3);
    BB_ASSERT(strcmp(spec.columns[0].name, "id") == 0);
    BB_ASSERT(strcmp(spec.columns[0].type, "TEXT") == 0 && spec.columns[0].primary_key == 1);
    BB_ASSERT(strcmp(spec.columns[1].type, "INTEGER") == 0 && spec.columns[1].primary_key == 0);
    BB_ASSERT(strcmp(spec.columns[2].type, "BLOB") == 0);

    BB_ASSERT(d->api->describe_table(d, "nope", &spec) == 0);
    BB_ASSERT(spec.count == 0);

    /* A hostile-looking table name is just a name: bound, not spliced. */
    BB_ASSERT(d->api->describe_table(d, "t'); DROP TABLE t; --", &spec) == 0);
    BB_ASSERT(spec.count == 0);
    BB_ASSERT(d->api->describe_table(d, "t", &spec) == 0 && spec.count == 3);

    bb_migrate_driver_close(d);
}

static void test_migrations_match_schema(void)
{
    printf("\tTesting migrations that match the schema produce no drift...\n");

    write_text_file("0001_create_tasks.sql",
        "CREATE TABLE IF NOT EXISTS tasks (id TEXT PRIMARY KEY, name TEXT, status TEXT);\n");
    write_text_file("task.schema.json", TASK_MANIFEST(""));

    const char *paths[] = { "0001_create_tasks.sql" };
    bb_migrate_driver_t *d = scratch_with(paths, 1);

    collector_t c = { "", 0 };
    BB_ASSERT(check(d, "task.schema.json", &c) == 0);
    BB_ASSERT(c.count == 0);

    bb_migrate_driver_close(d);
}

static void test_schema_changed_without_migration(void)
{
    printf("\tTesting a manifest edited without a migration is caught...\n");

    write_text_file("task_v2.schema.json",
        TASK_MANIFEST(", { \"name\": \"due\", \"type\": \"int\" }"));

    const char *paths[] = { "0001_create_tasks.sql" };
    bb_migrate_driver_t *d = scratch_with(paths, 1);

    collector_t c = { "", 0 };
    BB_ASSERT(check(d, "task_v2.schema.json", &c) == 1);
    BB_ASSERT(strstr(c.text, "'due' (INTEGER) is in the schema but no migration creates it") != NULL);

    bb_migrate_driver_close(d);
}

static void test_migration_added_and_schema_follows(void)
{
    printf("\tTesting migration + schema updated together agree (column order irrelevant)...\n");

    write_text_file("0002_add_due.sql", "ALTER TABLE tasks ADD COLUMN due INTEGER;\n");

    const char *paths[] = { "0001_create_tasks.sql", "0002_add_due.sql" };

    /* Migration added, manifest not yet updated: drift the other way. */
    bb_migrate_driver_t *d = scratch_with(paths, 2);
    collector_t c = { "", 0 };
    BB_ASSERT(check(d, "task.schema.json", &c) == 1);
    BB_ASSERT(strstr(c.text, "'due' is in the database but not in the schema") != NULL);

    /* Manifest updated too (due listed in the middle; ALTER put it last). */
    write_text_file("task_v2b.schema.json", 
        "{ \"kind\": \"persist.schema\", \"name\": \"Task\", \"table\": \"tasks\","
        "  \"fields\": ["
        "    { \"name\": \"id\", \"type\": \"uuid\", \"primary_key\": true },"
        "    { \"name\": \"due\", \"type\": \"int\" },"
        "    { \"name\": \"name\", \"type\": \"string\", \"size\": 64 },"
        "    { \"name\": \"status\", \"type\": \"string\", \"size\": 64 } ] }");

    collector_t c2 = { "", 0 };
    BB_ASSERT(check(d, "task_v2b.schema.json", &c2) == 0);

    bb_migrate_driver_close(d);
}

static void test_missing_table_and_wrong_type(void)
{
    printf("\tTesting missing table and wrong column type...\n");

    bb_migrate_driver_t *d = scratch();
    collector_t c = { "", 0 };
    BB_ASSERT(check(d, "task.schema.json", &c) == 1);
    BB_ASSERT(strstr(c.text, "table 'tasks' does not exist") != NULL);
    bb_migrate_driver_close(d);

    d = scratch();
    BB_ASSERT(d->api->exec(d, "CREATE TABLE tasks (id TEXT PRIMARY KEY, name INTEGER, status TEXT)") == 0);
    collector_t c2 = { "", 0 };
    BB_ASSERT(check(d, "task.schema.json", &c2) == 1);
    BB_ASSERT(strstr(c2.text, "'name' is TEXT in the schema but INTEGER in the database") != NULL);
    bb_migrate_driver_close(d);

    /* Lower-case / differently spelled declarations still compare equal. */
    d = scratch();
    BB_ASSERT(d->api->exec(d, "create table tasks (id text primary key, name text, status text)") == 0);
    collector_t c3 = { "", 0 };
    BB_ASSERT(check(d, "task.schema.json", &c3) == 0);
    bb_migrate_driver_close(d);
}

int main(void)
{
    printf("Running migration SQLite verify integration tests...\n");

    test_describe_table();
    test_migrations_match_schema();
    test_schema_changed_without_migration();
    test_migration_added_and_schema_follows();
    test_missing_table_and_wrong_type();

    remove("0001_create_tasks.sql");
    remove("0002_add_due.sql");
    remove("task.schema.json");
    remove("task_v2.schema.json");
    remove("task_v2b.schema.json");

    printf("All migration SQLite verify tests passed!\n");
    return 0;
}
