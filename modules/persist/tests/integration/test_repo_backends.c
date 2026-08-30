#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blue-bird/persist/repo.h"
#include "blue-bird/persist/model.h"
#include "blue-bird/persist/model/model_json.h"
#include "blue-bird/persist/model/model_sqlite.h"
#include "blue-bird/persist/query.h"
#include "blue-bird/persist/schema.h"

/* ---------------------------
 * Model
 * --------------------------- */

typedef struct {
    int id;
    char name[64];
    int age;
} User;

static bb_field_t fields[] = {
    {
        .name = "id",
        .type = BB_FIELD_INT,
        .offset = offsetof(User, id),
        .size = sizeof(int),
        .flags = BB_FIELD_NONE
    },
    {
        .name = "name",
        .type = BB_FIELD_STRING,
        .offset = offsetof(User, name),
        .size = 64,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "age",
        .type = BB_FIELD_INT,
        .offset = offsetof(User, age),
        .size = sizeof(int),
        .flags = BB_FIELD_NONE
    }
};

static bb_schema_t schema = {
    .name = "users",
    .fields = fields,
    .field_count = 3,
    .struct_size = sizeof(User),
    .primary_key_index = 0
};

/* ---------------------------
 * Helpers
 * --------------------------- */

static void cleanup(const char *path)
{
    remove(path);
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void run_tests(const char *file, const bb_model_api_t *api)
{
    bb_model_handle_t *h = api->open(file);
    BB_ASSERT(h);

    bb_repo_t repo;
    bb_repo_init(&repo, api, h, &schema);

    /* INSERT */
    printf("\tInsert...\n");
    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));
    BB_ASSERT(bb_repo_insert(&repo, &u) == 0);

    /* FIND */
    printf("\tFind...\n");
    User out = {0};
    int id = 1;
    BB_ASSERT(bb_repo_find_by_pk(&repo, &out, &id) == 0);
    BB_ASSERT(strcmp(out.name, "Alice") == 0);

    /* UPDATE */
    printf("\tUpdate...\n");
    strncpy(u.name, "Bob", sizeof(u.name));
    BB_ASSERT(bb_repo_update(&repo, &u) == 0);

    memset(&out, 0, sizeof(out));
    BB_ASSERT(bb_repo_find_by_pk(&repo, &out, &id) == 0);
    BB_ASSERT(strcmp(out.name, "Bob") == 0);

    /* REMOVE */
    printf("\tRemove...\n");
    BB_ASSERT(bb_repo_remove(&repo, &id) == 0);
    BB_ASSERT(bb_repo_find_by_pk(&repo, &out, &id) != 0);

    /* FIND ALL */
    printf("\tFind All...\n");
    printf("\tRepo find_all...\n");

    User u1 = { .id = 1 };
    strcpy(u1.name, "Alice");

    User u2 = { .id = 2 };
    strcpy(u2.name, "Bob");

    BB_ASSERT(bb_repo_insert(&repo, &u1) == 0);
    BB_ASSERT(bb_repo_insert(&repo, &u2) == 0);

    User *arr = NULL;
    size_t count = 0;

    BB_ASSERT(bb_repo_find_all(&repo, (void**)&arr, &count) == 0);

    BB_ASSERT(count == 2);

    /* order not guaranteed → just check existence */
    int found1 = 0, found2 = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (arr[i].id == 1) found1 = 1;
        if (arr[i].id == 2) found2 = 1;
    }

    BB_ASSERT(found1 && found2);

    free(arr);

    api->close(h);
}

/* ---------------------------
 * Query (bb_repo_find) tests
 *
 * Run against both backends with the exact same schema/data/queries:
 * SQLite implements query() natively (pushed down to SQL), JSON leaves
 * it NULL so bb_repo_find() falls back to find_all() + bb_query_apply().
 * Both must produce identical, correct results.
 * --------------------------- */

static void run_query_tests(const char *file, const bb_model_api_t *api)
{
    bb_model_handle_t *h = api->open(file);
    BB_ASSERT(h);

    bb_repo_t repo;
    bb_repo_init(&repo, api, h, &schema);

    User people[] = {
        { .id = 1, .name = "Alice", .age = 30 },
        { .id = 2, .name = "Bob",   .age = 17 },
        { .id = 3, .name = "Carol", .age = 40 },
        { .id = 4, .name = "Dave",  .age = 22 },
    };

    for (size_t i = 0; i < sizeof(people) / sizeof(people[0]); i++)
        BB_ASSERT(bb_repo_insert(&repo, &people[i]) == 0);

    /* WHERE age >= 18 -> everyone except Bob (17) */
    printf("\tWhere age >= 18...\n");
    {
        bb_query_t q;
        bb_query_init(&q, &schema);
        int min_age = 18;
        BB_ASSERT(bb_query_where(&q, "age", BB_OP_GTE, &min_age) == 0);

        User *out = NULL;
        size_t count = 0;
        BB_ASSERT(bb_repo_find(&repo, &q, (void **)&out, &count) == 0);
        BB_ASSERT(count == 3);

        int found_bob = 0;
        for (size_t i = 0; i < count; i++)
            if (out[i].id == 2) found_bob = 1;
        BB_ASSERT(!found_bob);

        free(out);
    }

    /* Same filter, but ORDER BY age ASC -> Dave(22), Alice(30), Carol(40) */
    printf("\tWhere age >= 18, order by age asc...\n");
    {
        bb_query_t q;
        bb_query_init(&q, &schema);
        int min_age = 18;
        BB_ASSERT(bb_query_where(&q, "age", BB_OP_GTE, &min_age) == 0);
        BB_ASSERT(bb_query_order_by(&q, "age", BB_ORDER_ASC) == 0);

        User *out = NULL;
        size_t count = 0;
        BB_ASSERT(bb_repo_find(&repo, &q, (void **)&out, &count) == 0);
        BB_ASSERT(count == 3);
        BB_ASSERT(out[0].id == 4); /* Dave, 22 */
        BB_ASSERT(out[1].id == 1); /* Alice, 30 */
        BB_ASSERT(out[2].id == 3); /* Carol, 40 */

        free(out);
    }

    /* Same, but LIMIT 1 OFFSET 1 -> just Alice (30) */
    printf("\tWhere age >= 18, order by age asc, limit 1 offset 1...\n");
    {
        bb_query_t q;
        bb_query_init(&q, &schema);
        int min_age = 18;
        BB_ASSERT(bb_query_where(&q, "age", BB_OP_GTE, &min_age) == 0);
        BB_ASSERT(bb_query_order_by(&q, "age", BB_ORDER_ASC) == 0);
        bb_query_limit(&q, 1);
        bb_query_offset(&q, 1);

        User *out = NULL;
        size_t count = 0;
        BB_ASSERT(bb_repo_find(&repo, &q, (void **)&out, &count) == 0);
        BB_ASSERT(count == 1);
        BB_ASSERT(out[0].id == 1); /* Alice */

        free(out);
    }

    /* LIKE on name */
    printf("\tWhere name LIKE 'A%%'...\n");
    {
        bb_query_t q;
        bb_query_init(&q, &schema);
        BB_ASSERT(bb_query_where(&q, "name", BB_OP_LIKE, "A%") == 0);

        User *out = NULL;
        size_t count = 0;
        BB_ASSERT(bb_repo_find(&repo, &q, (void **)&out, &count) == 0);
        BB_ASSERT(count == 1);
        BB_ASSERT(strcmp(out[0].name, "Alice") == 0);

        free(out);
    }

    /* No matches -> 0 rows, not an error */
    printf("\tWhere age >= 999 (no matches)...\n");
    {
        bb_query_t q;
        bb_query_init(&q, &schema);
        int impossible_age = 999;
        BB_ASSERT(bb_query_where(&q, "age", BB_OP_GTE, &impossible_age) == 0);

        User *out = NULL;
        size_t count = 0;
        BB_ASSERT(bb_repo_find(&repo, &q, (void **)&out, &count) == 0);
        BB_ASSERT(count == 0);
        BB_ASSERT(out == NULL);
    }

    api->close(h);
}

static void test_repo_json_crud(void)
{
    printf("JSON repo CRUD...\n");

    const char *file = "test_repo_json.json";
    cleanup(file);

    BB_ASSERT(bb_model_register(bb_model_json_api()) == 0);

    const bb_model_api_t *api = bb_model_get("json");
    BB_ASSERT(api);

    run_tests(file, api);

    printf("JSON Tests passed...\n");
}

static void test_repo_sqlite_crud(void)
{
    printf("SQLite repo CRUD...\n");

    const char *db = "test_repo_sqlite.db";
    cleanup(db);

    BB_ASSERT(bb_model_register(bb_model_sqlite_api()) == 0);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api);

    run_tests(db, api);

    printf("SQLite Tests passed...\n");
}

static void test_repo_json_query(void)
{
    printf("JSON repo query (fallback path, api->query == NULL)...\n");

    const char *file = "test_repo_json_query.json";
    cleanup(file);

    const bb_model_api_t *api = bb_model_get("json");
    BB_ASSERT(api);
    BB_ASSERT(api->query == NULL); /* confirms this really exercises the fallback */

    run_query_tests(file, api);

    printf("JSON query tests passed...\n");
}

static void test_repo_sqlite_query(void)
{
    printf("SQLite repo query (native pushdown)...\n");

    const char *db = "test_repo_sqlite_query.db";
    cleanup(db);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api);
    BB_ASSERT(api->query != NULL); /* confirms this really exercises the pushdown path */

    run_query_tests(db, api);

    printf("SQLite query tests passed...\n");
}

int main(void)
{
    printf("Running repo integration tests...\n");

    test_repo_json_crud();
    test_repo_sqlite_crud();
    test_repo_json_query();
    test_repo_sqlite_query();

    printf("All repo tests passed!\n");
    return 0;
}
