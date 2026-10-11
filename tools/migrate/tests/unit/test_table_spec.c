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

/* Collects diff messages into one string for easy substring checks. */
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

static void add_col(bb_migrate_table_spec_t *s, const char *name, const char *type, int pk)
{
    bb_migrate_column_t *c = &s->columns[s->count++];
    snprintf(c->name, sizeof c->name, "%s", name);
    snprintf(c->type, sizeof c->type, "%s", type);
    c->primary_key = pk;
}

static void init_spec(bb_migrate_table_spec_t *s, const char *table)
{
    memset(s, 0, sizeof *s);
    snprintf(s->table, sizeof s->table, "%s", table);
}

/* ---------------------------
 * Manifest loading
 * --------------------------- */

static void test_manifest_loading(void)
{
    printf("\tTesting manifest -> expected columns...\n");

    write_text_file("test_spec_task.schema.json",
        "{ \"version\": 1, \"kind\": \"persist.schema\", \"name\": \"Task\", \"table\": \"tasks\","
        "  \"fields\": ["
        "    { \"name\": \"id\", \"type\": \"uuid\", \"primary_key\": true },"
        "    { \"name\": \"name\", \"type\": \"string\", \"size\": 64 },"
        "    { \"name\": \"n\", \"type\": \"int\" },"
        "    { \"name\": \"data\", \"type\": \"blob\", \"size\": 16 } ] }");

    bb_migrate_table_spec_t spec;
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_spec_from_manifest("test_spec_task.schema.json", &spec, &err) == 0);
    BB_ASSERT(strcmp(spec.table, "tasks") == 0);
    BB_ASSERT(spec.count == 4);
    BB_ASSERT(strcmp(spec.columns[0].name, "id") == 0);
    BB_ASSERT(strcmp(spec.columns[0].type, "TEXT") == 0 && spec.columns[0].primary_key == 1);
    BB_ASSERT(strcmp(spec.columns[1].type, "TEXT") == 0 && spec.columns[1].primary_key == 0);
    BB_ASSERT(strcmp(spec.columns[2].type, "INTEGER") == 0);
    BB_ASSERT(strcmp(spec.columns[3].type, "BLOB") == 0);

    /* "table" is optional: falls back to "name", like bb-codegen. */
    write_text_file("test_spec_noname.schema.json",
        "{ \"kind\": \"persist.schema\", \"name\": \"Note\","
        "  \"fields\": [ { \"name\": \"id\", \"type\": \"int\", \"primary_key\": true } ] }");
    BB_ASSERT(bb_migrate_spec_from_manifest("test_spec_noname.schema.json", &spec, &err) == 0);
    BB_ASSERT(strcmp(spec.table, "Note") == 0);

    remove("test_spec_task.schema.json");
    remove("test_spec_noname.schema.json");
}

static void test_manifest_errors(void)
{
    printf("\tTesting manifest errors...\n");

    bb_migrate_table_spec_t spec;
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_spec_from_manifest("test_spec_does_not_exist.json", &spec, &err) != 0);
    BB_ASSERT(strstr(err.msg, "cannot read manifest") != NULL);

    const char *bad[] = {
        "{ \"kind\": \"web.route\", \"name\": \"X\", \"fields\": [] }",
        "{ \"kind\": \"persist.schema\", \"name\": \"X\" }",
        "{ \"kind\": \"persist.schema\", \"name\": \"X\", \"fields\": [] }",
        "{ \"kind\": \"persist.schema\", \"name\": \"X\", \"fields\": [ { \"type\": \"int\" } ] }",
        "{ \"kind\": \"persist.schema\", \"name\": \"X\", \"fields\": [ { \"name\": \"a\", \"type\": \"float\" } ] }",
        "{ \"kind\": \"persist.schema\", \"fields\": [ { \"name\": \"a\", \"type\": \"int\" } ] }",
        "not json at all",
    };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        write_text_file("test_spec_bad.json", bad[i]);
        err.msg[0] = '\0';
        BB_ASSERT(bb_migrate_spec_from_manifest("test_spec_bad.json", &spec, &err) != 0);
        BB_ASSERT(err.msg[0] != '\0');
        BB_ASSERT(spec.count == 0);
    }

    remove("test_spec_bad.json");
}

/* ---------------------------
 * Comparison
 * --------------------------- */

static void test_compare_match_ignores_order(void)
{
    printf("\tTesting identical shapes match, regardless of column order...\n");

    bb_migrate_table_spec_t exp, act;
    init_spec(&exp, "tasks");
    add_col(&exp, "id", "TEXT", 1);
    add_col(&exp, "name", "TEXT", 0);
    add_col(&exp, "status", "TEXT", 0);

    /* ALTER TABLE ADD COLUMN puts "status" last; here it's first. */
    init_spec(&act, "tasks");
    add_col(&act, "status", "TEXT", 0);
    add_col(&act, "id", "TEXT", 1);
    add_col(&act, "name", "TEXT", 0);

    collector_t c = { "", 0 };
    BB_ASSERT(bb_migrate_spec_compare(&exp, &act, collect, &c) == 0);
    BB_ASSERT(c.count == 0);
    BB_ASSERT(bb_migrate_spec_compare(&exp, &act, NULL, NULL) == 0); /* callback optional */
}

static void test_compare_differences(void)
{
    printf("\tTesting each kind of difference is reported...\n");

    bb_migrate_table_spec_t exp, act;
    init_spec(&exp, "tasks");
    add_col(&exp, "id", "TEXT", 1);
    add_col(&exp, "name", "TEXT", 0);
    add_col(&exp, "age", "INTEGER", 0);
    add_col(&exp, "missing_col", "TEXT", 0);

    init_spec(&act, "tasks");
    add_col(&act, "id", "TEXT", 0);          /* lost its primary key */
    add_col(&act, "name", "INTEGER", 0);     /* wrong type */
    add_col(&act, "age", "INTEGER", 0);      /* fine */
    add_col(&act, "extra", "TEXT", 1);       /* not in schema, and a pk */

    collector_t c = { "", 0 };
    size_t n = bb_migrate_spec_compare(&exp, &act, collect, &c);

    BB_ASSERT(n == 4);
    BB_ASSERT((int)n == c.count);
    BB_ASSERT(strstr(c.text, "'id' is the primary key in the schema but not in the database") != NULL);
    BB_ASSERT(strstr(c.text, "'name' is TEXT in the schema but INTEGER in the database") != NULL);
    BB_ASSERT(strstr(c.text, "'missing_col' (TEXT) is in the schema but no migration creates it") != NULL);
    /* An extra column is reported once, even though it is also a pk. */
    BB_ASSERT(strstr(c.text, "'extra' is in the database but not in the schema") != NULL);
    BB_ASSERT(strstr(c.text, "'extra' is a primary key") == NULL);
    BB_ASSERT(strstr(c.text, "'age'") == NULL);
}

static void test_compare_missing_table(void)
{
    printf("\tTesting a missing table is one clear difference...\n");

    bb_migrate_table_spec_t exp, act;
    init_spec(&exp, "tasks");
    add_col(&exp, "id", "TEXT", 1);
    init_spec(&act, "tasks");

    collector_t c = { "", 0 };
    BB_ASSERT(bb_migrate_spec_compare(&exp, &act, collect, &c) == 1);
    BB_ASSERT(strstr(c.text, "table 'tasks' does not exist") != NULL);
}

int main(void)
{
    printf("Running migration table spec unit tests...\n");

    test_manifest_loading();
    test_manifest_errors();
    test_compare_match_ignores_order();
    test_compare_differences();
    test_compare_missing_table();

    printf("All migration table spec tests passed!\n");
    return 0;
}
