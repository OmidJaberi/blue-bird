#include "generate.h"

#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void init_spec(bb_migrate_table_spec_t *s, const char *table)
{
    memset(s, 0, sizeof *s);
    snprintf(s->table, sizeof s->table, "%s", table);
}

static void add_col(bb_migrate_table_spec_t *s, const char *name, const char *type, int pk)
{
    bb_migrate_column_t *c = &s->columns[s->count++];
    snprintf(c->name, sizeof c->name, "%s", name);
    snprintf(c->type, sizeof c->type, "%s", type);
    c->primary_key = pk;
}

static void task_spec(bb_migrate_table_spec_t *s)
{
    init_spec(s, "tasks");
    add_col(s, "id", "TEXT", 1);
    add_col(s, "name", "TEXT", 0);
    add_col(s, "status", "TEXT", 0);
}

static void test_create_table(void)
{
    printf("\tTesting a missing table becomes CREATE TABLE (Persist's shape)...\n");

    bb_migrate_table_spec_t exp, act;
    task_spec(&exp);
    init_spec(&act, "tasks");

    char *sql = NULL;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, NULL) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(sql != NULL);
    BB_ASSERT(strcmp(sql,
        "CREATE TABLE IF NOT EXISTS tasks (\n"
        "    id TEXT PRIMARY KEY,\n"
        "    name TEXT,\n"
        "    status TEXT\n"
        ");\n") == 0);
    free(sql);
}

static void test_add_columns(void)
{
    printf("\tTesting new columns become ALTER TABLE ADD COLUMN, in schema order...\n");

    bb_migrate_table_spec_t exp, act;
    task_spec(&exp);
    add_col(&exp, "due", "INTEGER", 0);
    add_col(&exp, "blob_data", "BLOB", 0);

    task_spec(&act);

    char *sql = NULL;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, NULL) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strcmp(sql,
        "ALTER TABLE tasks ADD COLUMN due INTEGER;\n"
        "ALTER TABLE tasks ADD COLUMN blob_data BLOB;\n") == 0);
    free(sql);
}

static void test_nothing_to_do(void)
{
    printf("\tTesting matching shapes generate nothing...\n");

    bb_migrate_table_spec_t exp, act;
    task_spec(&exp);
    task_spec(&act);

    char *sql = (char *)0x1;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, NULL) == BB_MIGRATE_GEN_NOTHING);
    BB_ASSERT(sql == NULL);
}

static void test_refusals(void)
{
    printf("\tTesting destructive/ambiguous changes are refused...\n");

    bb_migrate_table_spec_t exp, act;
    bb_migrate_err_t err;
    char *sql = NULL;

    /* A column only in the database: drop or rename? Unknowable. */
    task_spec(&exp);
    task_spec(&act);
    add_col(&act, "legacy", "TEXT", 0);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);
    BB_ASSERT(strstr(err.msg, "'legacy'") != NULL);
    BB_ASSERT(strstr(err.msg, "dropped or a renamed column") != NULL);
    BB_ASSERT(strstr(err.msg, "write this migration by hand") != NULL);

    /* A rename looks like add + extra: refused, NOT half-generated. */
    task_spec(&exp);
    init_spec(&act, "tasks");
    add_col(&act, "id", "TEXT", 1);
    add_col(&act, "name", "TEXT", 0);
    add_col(&act, "state", "TEXT", 0);     /* was "status" */
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);
    BB_ASSERT(strstr(err.msg, "'state'") != NULL);

    /* Type change. */
    task_spec(&exp);
    task_spec(&act);
    strcpy(act.columns[1].type, "INTEGER");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "'name' is TEXT in the schema but INTEGER") != NULL);
    BB_ASSERT(strstr(err.msg, "table rebuild") != NULL);

    /* Primary key changes, both directions. */
    task_spec(&exp);
    task_spec(&act);
    act.columns[0].primary_key = 0;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "primary key") != NULL);

    /* Adding a new primary key column. */
    task_spec(&exp);
    add_col(&exp, "uid", "TEXT", 1);
    task_spec(&act);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "can't add a primary key column") != NULL);
}

static void test_mixed_is_all_or_nothing(void)
{
    printf("\tTesting one refusal blocks the whole table (no partial SQL)...\n");

    bb_migrate_table_spec_t exp, act;
    task_spec(&exp);
    add_col(&exp, "due", "INTEGER", 0);   /* generatable */

    task_spec(&act);
    add_col(&act, "legacy", "TEXT", 0);   /* refused */

    bb_migrate_err_t err;
    char *sql = NULL;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);
    BB_ASSERT(strstr(err.msg, "'legacy'") != NULL);
    BB_ASSERT(strstr(err.msg, "ADD COLUMN") == NULL);
}

static void test_many_refusals_stay_bounded(void)
{
    printf("\tTesting a flood of refusals is summarised, not overflowed...\n");

    bb_migrate_table_spec_t exp, act;
    init_spec(&exp, "t");
    add_col(&exp, "id", "TEXT", 1);

    init_spec(&act, "t");
    add_col(&act, "id", "TEXT", 1);

    for (int i = 0; i < 40; i++)
    {
        char name[32];
        snprintf(name, sizeof name, "extra_column_%d", i);
        add_col(&act, name, "TEXT", 0);
    }

    bb_migrate_err_t err;
    char *sql = NULL;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strlen(err.msg) < sizeof err.msg);
    BB_ASSERT(strstr(err.msg, "more)") != NULL);
    BB_ASSERT(strstr(err.msg, "write this migration by hand") != NULL);
}

static void test_unsafe_identifiers(void)
{
    printf("\tTesting non-plain identifiers are refused, never spliced into SQL...\n");

    bb_migrate_table_spec_t exp, act;
    bb_migrate_err_t err;
    char *sql = NULL;

    init_spec(&exp, "tasks; DROP TABLE users");
    add_col(&exp, "id", "TEXT", 1);
    init_spec(&act, "tasks; DROP TABLE users");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);

    task_spec(&exp);
    add_col(&exp, "bad col", "TEXT", 0);
    task_spec(&act);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);

    task_spec(&exp);
    add_col(&exp, "1st", "TEXT", 0);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
}

int main(void)
{
    printf("Running migration generator unit tests...\n");

    test_create_table();
    test_add_columns();
    test_nothing_to_do();
    test_refusals();
    test_mixed_is_all_or_nothing();
    test_many_refusals_stay_bounded();
    test_unsafe_identifiers();

    printf("All migration generator tests passed!\n");
    return 0;
}
