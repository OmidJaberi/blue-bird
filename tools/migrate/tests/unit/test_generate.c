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
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, NULL) == BB_MIGRATE_GEN_OK);
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
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, NULL) == BB_MIGRATE_GEN_OK);
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
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, NULL) == BB_MIGRATE_GEN_NOTHING);
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
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);
    BB_ASSERT(strstr(err.msg, "'legacy'") != NULL);
    BB_ASSERT(strstr(err.msg, "dropping a column can lose data") != NULL);
    BB_ASSERT(strstr(err.msg, "--rename") == NULL); /* nothing it could have been renamed to */
    BB_ASSERT(strstr(err.msg, "write this migration by hand") != NULL);

    /* A rename looks like add + extra: refused, NOT half-generated. */
    task_spec(&exp);
    init_spec(&act, "tasks");
    add_col(&act, "id", "TEXT", 1);
    add_col(&act, "name", "TEXT", 0);
    add_col(&act, "state", "TEXT", 0);     /* was "status" */
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);
    BB_ASSERT(strstr(err.msg, "'state'") != NULL);
    /* Exactly one plausible target, so the refusal suggests the precise hint. */
    BB_ASSERT(strstr(err.msg, "add --rename tasks.state=status") != NULL);

    /* Type change. */
    task_spec(&exp);
    task_spec(&act);
    strcpy(act.columns[1].type, "INTEGER");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "'name' is TEXT in the schema but INTEGER") != NULL);
    BB_ASSERT(strstr(err.msg, "table rebuild") != NULL);

    /* Primary key changes, both directions. */
    task_spec(&exp);
    task_spec(&act);
    act.columns[0].primary_key = 0;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "primary key") != NULL);

    /* Adding a new primary key column. */
    task_spec(&exp);
    add_col(&exp, "uid", "TEXT", 1);
    task_spec(&act);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
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
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
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
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
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
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);

    task_spec(&exp);
    add_col(&exp, "bad col", "TEXT", 0);
    task_spec(&act);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);

    task_spec(&exp);
    add_col(&exp, "1st", "TEXT", 0);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
}


/* ---------------------------
 * --rename
 * --------------------------- */

static void rename_hint(bb_migrate_rename_t *r, const char *table, const char *from, const char *to)
{
    memset(r, 0, sizeof *r);
    snprintf(r->table, sizeof r->table, "%s", table);
    snprintf(r->from, sizeof r->from, "%s", from);
    snprintf(r->to, sizeof r->to, "%s", to);
}

/* Schema: id, name, state.  Database: id, name, status. */
static void renamed_specs(bb_migrate_table_spec_t *exp, bb_migrate_table_spec_t *act)
{
    init_spec(exp, "tasks");
    add_col(exp, "id", "TEXT", 1);
    add_col(exp, "name", "TEXT", 0);
    add_col(exp, "state", "TEXT", 0);

    task_spec(act); /* id, name, status */
}

static void test_parse_rename(void)
{
    printf("\tTesting --rename argument parsing...\n");

    bb_migrate_rename_t r;
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_parse_rename("tasks.status=state", &r, &err) == 0);
    BB_ASSERT(strcmp(r.table, "tasks") == 0);
    BB_ASSERT(strcmp(r.from, "status") == 0);
    BB_ASSERT(strcmp(r.to, "state") == 0);

    const char *bad[] = {
        "", "status=state", "tasks.status", "tasks.=state", "tasks.status=", ".status=state",
        "tasks.status=state=x", "a.b.c=d", "tasks.status=sta te", "tasks.sta tus=state",
        "tasks.status=st-ate", "tasks.status=1abc", "ta;sks.status=state", "tasks.status=a.b",
        "tasks=status.state",
    };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        err.msg[0] = '\0';
        BB_ASSERT(bb_migrate_parse_rename(bad[i], &r, &err) != 0);
        BB_ASSERT(strstr(err.msg, "--rename expects") != NULL);
        BB_ASSERT(r.table[0] == '\0'); /* cleared on failure */
    }

    /* Over-long identifier. */
    char longarg[400];
    snprintf(longarg, sizeof longarg, "tasks.%.200s=state", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    BB_ASSERT(bb_migrate_parse_rename(longarg, &r, &err) != 0);
}

static void test_rename_generates_alter(void)
{
    printf("\tTesting a rename becomes ALTER TABLE ... RENAME COLUMN...\n");

    bb_migrate_table_spec_t exp, act;
    renamed_specs(&exp, &act);

    bb_migrate_rename_t hint;
    rename_hint(&hint, "tasks", "status", "state");

    char *sql = NULL;
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strcmp(sql, "ALTER TABLE tasks RENAME COLUMN status TO state;\n") == 0);
    free(sql);
}

static void test_rename_plus_add_orders_rename_first(void)
{
    printf("\tTesting renames are listed before adds, and name reuse is refused as ambiguous...\n");

    /* Schema: id, name, state, status(NEW, INTEGER).  Database: id, name, status(TEXT). */
    bb_migrate_table_spec_t exp, act;
    init_spec(&exp, "tasks");
    add_col(&exp, "id", "TEXT", 1);
    add_col(&exp, "name", "TEXT", 0);
    add_col(&exp, "state", "TEXT", 0);
    add_col(&exp, "status", "INTEGER", 0);
    task_spec(&act);

    /* Reusing the old name for a new column: "status" is still in the schema,
     * so this could equally be a type change. Refused, not guessed. */
    bb_migrate_rename_t hint;
    rename_hint(&hint, "tasks", "status", "state");
    char *sql = NULL;
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "the schema still has a column 'status'") != NULL);

    /* A plain rename plus an unrelated new column: both emitted, rename first. */
    renamed_specs(&exp, &act);
    add_col(&exp, "due", "INTEGER", 0);

    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &(bb_migrate_rename_t){ "tasks", "status", "state" }, 1,
                                      &sql, &err) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strcmp(sql,
        "ALTER TABLE tasks RENAME COLUMN status TO state;\n"
        "ALTER TABLE tasks ADD COLUMN due INTEGER;\n") == 0);
    free(sql);

    /* Two renames in one migration, hint order preserved. */
    init_spec(&exp, "tasks");
    add_col(&exp, "id", "TEXT", 1);
    add_col(&exp, "title", "TEXT", 0);
    add_col(&exp, "state", "TEXT", 0);
    task_spec(&act);

    bb_migrate_rename_t two[2];
    rename_hint(&two[0], "tasks", "status", "state");
    rename_hint(&two[1], "tasks", "name", "title");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, two, 2, &sql, &err) == BB_MIGRATE_GEN_OK);
    BB_ASSERT(strcmp(sql,
        "ALTER TABLE tasks RENAME COLUMN status TO state;\n"
        "ALTER TABLE tasks RENAME COLUMN name TO title;\n") == 0);
    free(sql);
}

static void test_inconsistent_hints_are_errors(void)
{
    printf("\tTesting hints that don't match the real difference are refused...\n");

    bb_migrate_table_spec_t exp, act;
    bb_migrate_rename_t hint;
    bb_migrate_err_t err;
    char *sql = NULL;

    /* The database has no such column. */
    renamed_specs(&exp, &act);
    rename_hint(&hint, "tasks", "nonexistent", "state");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL);
    BB_ASSERT(strstr(err.msg, "the database has no column 'nonexistent'") != NULL);

    /* The schema has no such target. */
    renamed_specs(&exp, &act);
    rename_hint(&hint, "tasks", "status", "elsewhere");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "the schema has no column 'elsewhere'") != NULL);

    /* The target already exists in the database. */
    renamed_specs(&exp, &act);
    add_col(&act, "state", "TEXT", 0);
    rename_hint(&hint, "tasks", "status", "state");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "the database already has a column 'state'") != NULL);

    /* A rename that also changes the type is two changes: not generated. */
    renamed_specs(&exp, &act);
    strcpy(exp.columns[2].type, "INTEGER");
    rename_hint(&hint, "tasks", "status", "state");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "the type or primary key also changes (TEXT -> INTEGER)") != NULL);

    /* Nothing to rename at all: schema already matches. */
    task_spec(&exp);
    task_spec(&act);
    rename_hint(&hint, "tasks", "status", "state");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_REFUSED);

    /* Two hints fighting over the same column. */
    renamed_specs(&exp, &act);
    add_col(&exp, "other", "TEXT", 0);
    bb_migrate_rename_t two[2];
    rename_hint(&two[0], "tasks", "status", "state");
    rename_hint(&two[1], "tasks", "status", "other");
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, two, 2, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "conflicts with another --rename") != NULL);
    BB_ASSERT(sql == NULL);
}

static void test_valid_rename_does_not_excuse_other_problems(void)
{
    printf("\tTesting a valid rename doesn't excuse an unrelated refused change...\n");

    bb_migrate_table_spec_t exp, act;
    renamed_specs(&exp, &act);
    strcpy(act.columns[1].type, "INTEGER"); /* "name" also changed type */

    bb_migrate_rename_t hint;
    rename_hint(&hint, "tasks", "status", "state");

    bb_migrate_err_t err;
    char *sql = NULL;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(sql == NULL); /* all or nothing: not even the rename */
    BB_ASSERT(strstr(err.msg, "'name' is TEXT in the schema but INTEGER") != NULL);
    BB_ASSERT(strstr(err.msg, "RENAME") == NULL);
}

static void test_hints_for_other_tables_are_ignored_here(void)
{
    printf("\tTesting hints for other tables don't affect this one...\n");

    bb_migrate_table_spec_t exp, act;
    task_spec(&exp);
    task_spec(&act);

    bb_migrate_rename_t hint;
    rename_hint(&hint, "notes", "a", "b");

    char *sql = (char *)0x1;
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, &hint, 1, &sql, NULL) == BB_MIGRATE_GEN_NOTHING);
    BB_ASSERT(sql == NULL);
}

static void test_suggestions(void)
{
    printf("\tTesting refusals suggest a rename only when it is unambiguous...\n");

    bb_migrate_table_spec_t exp, act;
    bb_migrate_err_t err;
    char *sql = NULL;

    /* Two schema-only TEXT columns could each be the target: generic hint. */
    init_spec(&exp, "tasks");
    add_col(&exp, "id", "TEXT", 1);
    add_col(&exp, "alpha", "TEXT", 0);
    add_col(&exp, "beta", "TEXT", 0);
    init_spec(&act, "tasks");
    add_col(&act, "id", "TEXT", 1);
    add_col(&act, "old", "TEXT", 0);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "--rename tasks.old=<new name>") != NULL);

    /* The only schema-only column has a different type: not a rename candidate. */
    init_spec(&exp, "tasks");
    add_col(&exp, "id", "TEXT", 1);
    add_col(&exp, "count", "INTEGER", 0);
    BB_ASSERT(bb_migrate_generate_sql(&exp, &act, NULL, 0, &sql, &err) == BB_MIGRATE_GEN_REFUSED);
    BB_ASSERT(strstr(err.msg, "--rename") == NULL);
    BB_ASSERT(strstr(err.msg, "dropping a column can lose data") != NULL);
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
    test_parse_rename();
    test_rename_generates_alter();
    test_rename_plus_add_orders_rename_first();
    test_inconsistent_hints_are_errors();
    test_valid_rename_does_not_excuse_other_problems();
    test_hints_for_other_tables_are_ignored_here();
    test_suggestions();

    printf("All migration generator tests passed!\n");
    return 0;
}
