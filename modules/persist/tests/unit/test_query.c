#include "blue-bird/persist/query.h"
#include "blue-bird/persist/schema.h"
#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------
 * Test Model
 * --------------------------- */

typedef struct {
    int id;
    char name[32];
    int age;
} Person;

static bb_field_t person_fields[] = {
    {
        .name = "id",
        .type = BB_FIELD_INT,
        .offset = offsetof(Person, id),
        .size = sizeof(int),
        .flags = BB_FIELD_NONE
    },
    {
        .name = "name",
        .type = BB_FIELD_STRING,
        .offset = offsetof(Person, name),
        .size = 32,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "age",
        .type = BB_FIELD_INT,
        .offset = offsetof(Person, age),
        .size = sizeof(int),
        .flags = BB_FIELD_NONE
    }
};

static bb_schema_t person_schema = {
    .name = "people",
    .fields = person_fields,
    .field_count = 3,
    .struct_size = sizeof(Person),
    .primary_key_index = 0
};

static Person make(int id, const char *name, int age)
{
    Person p = { .id = id, .age = age };
    strncpy(p.name, name, sizeof(p.name) - 1);
    return p;
}

/* ---------------------------
 * Builder tests
 * --------------------------- */

static void test_builder_basic(void)
{
    printf("\tTesting builder accumulates conditions/sorts...\n");

    bb_query_t q;
    bb_query_init(&q, &person_schema);

    BB_ASSERT(q.condition_count == 0);
    BB_ASSERT(q.sort_count == 0);
    BB_ASSERT(q.limit == -1);
    BB_ASSERT(q.offset == -1);

    int min_age = 18;
    BB_ASSERT(bb_query_where(&q, "age", BB_OP_GTE, &min_age) == 0);
    BB_ASSERT(q.condition_count == 1);

    BB_ASSERT(bb_query_order_by(&q, "name", BB_ORDER_ASC) == 0);
    BB_ASSERT(q.sort_count == 1);

    bb_query_limit(&q, 10);
    bb_query_offset(&q, 5);
    BB_ASSERT(q.limit == 10);
    BB_ASSERT(q.offset == 5);
}

static void test_builder_rejects_unknown_field(void)
{
    printf("\tTesting builder rejects unknown field names...\n");

    bb_query_t q;
    bb_query_init(&q, &person_schema);

    int val = 1;
    BB_ASSERT(bb_query_where(&q, "does_not_exist", BB_OP_EQ, &val) != 0);
    BB_ASSERT(q.condition_count == 0);

    BB_ASSERT(bb_query_order_by(&q, "does_not_exist", BB_ORDER_ASC) != 0);
    BB_ASSERT(q.sort_count == 0);
}

static void test_builder_condition_capacity(void)
{
    printf("\tTesting builder enforces condition/sort capacity...\n");

    bb_query_t q;
    bb_query_init(&q, &person_schema);

    int val = 1;

    for (int i = 0; i < BB_QUERY_MAX_CONDITIONS; i++)
        BB_ASSERT(bb_query_where(&q, "age", BB_OP_NE, &val) == 0);

    /* one more than the array can hold */
    BB_ASSERT(bb_query_where(&q, "age", BB_OP_NE, &val) != 0);
    BB_ASSERT(q.condition_count == BB_QUERY_MAX_CONDITIONS);
}

/* ---------------------------
 * bb_query_matches tests
 * --------------------------- */

static void test_matches_eq_and_multiple_conditions(void)
{
    printf("\tTesting bb_query_matches with EQ and multiple AND'd conditions...\n");

    Person alice = make(1, "Alice", 30);

    bb_query_t q;
    bb_query_init(&q, &person_schema);

    int id = 1;
    BB_ASSERT(bb_query_where(&q, "id", BB_OP_EQ, &id) == 0);
    BB_ASSERT(bb_query_matches(&alice, &q) == 1);

    int wrong_id = 2;
    bb_query_t q2;
    bb_query_init(&q2, &person_schema);
    BB_ASSERT(bb_query_where(&q2, "id", BB_OP_EQ, &wrong_id) == 0);
    BB_ASSERT(bb_query_matches(&alice, &q2) == 0);

    /* two conditions ANDed together */
    bb_query_t q3;
    bb_query_init(&q3, &person_schema);
    int min_age = 18;
    BB_ASSERT(bb_query_where(&q3, "id", BB_OP_EQ, &id) == 0);
    BB_ASSERT(bb_query_where(&q3, "age", BB_OP_GTE, &min_age) == 0);
    BB_ASSERT(bb_query_matches(&alice, &q3) == 1);

    int too_old = 40;
    bb_query_t q4;
    bb_query_init(&q4, &person_schema);
    BB_ASSERT(bb_query_where(&q4, "id", BB_OP_EQ, &id) == 0);
    BB_ASSERT(bb_query_where(&q4, "age", BB_OP_GTE, &too_old) == 0);
    BB_ASSERT(bb_query_matches(&alice, &q4) == 0); /* id matches, age doesn't -> AND fails */
}

static void test_matches_comparison_ops(void)
{
    printf("\tTesting bb_query_matches with LT/LTE/GT/GTE/NE...\n");

    Person p = make(1, "Bob", 25);

    struct { bb_query_op_t op; int threshold; int expect; } cases[] = {
        { BB_OP_LT,  30, 1 },
        { BB_OP_LT,  25, 0 },
        { BB_OP_LTE, 25, 1 },
        { BB_OP_GT,  20, 1 },
        { BB_OP_GT,  25, 0 },
        { BB_OP_GTE, 25, 1 },
        { BB_OP_NE,  25, 0 },
        { BB_OP_NE,  99, 1 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bb_query_t q;
        bb_query_init(&q, &person_schema);
        BB_ASSERT(bb_query_where(&q, "age", cases[i].op, &cases[i].threshold) == 0);
        BB_ASSERT(bb_query_matches(&p, &q) == cases[i].expect);
    }
}

static void test_matches_like(void)
{
    printf("\tTesting bb_query_matches with LIKE wildcards...\n");

    Person p = make(1, "Alexandra", 25);

    struct { const char *pattern; int expect; } cases[] = {
        { "Alex%",       1 },
        { "%andra",      1 },
        { "%ex%",        1 },
        { "Alexandra",   1 },
        { "Bob%",        0 },
        { "%zzz%",       0 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bb_query_t q;
        bb_query_init(&q, &person_schema);
        const char *pattern = cases[i].pattern;
        BB_ASSERT(bb_query_where(&q, "name", BB_OP_LIKE, pattern) == 0);
        BB_ASSERT(bb_query_matches(&p, &q) == cases[i].expect);
    }
}

/* ---------------------------
 * bb_query_apply tests (filter + sort + limit/offset)
 * --------------------------- */

static void test_apply_filters(void)
{
    printf("\tTesting bb_query_apply filters rows...\n");

    Person people[] = {
        make(1, "Alice", 30),
        make(2, "Bob", 17),
        make(3, "Carol", 40),
    };

    bb_query_t q;
    bb_query_init(&q, &person_schema);
    int min_age = 18;
    BB_ASSERT(bb_query_where(&q, "age", BB_OP_GTE, &min_age) == 0);

    Person *out = NULL;
    size_t count = 0;
    BB_ASSERT(bb_query_apply(&q, people, 3, (void **)&out, &count) == 0);
    BB_ASSERT(count == 2);

    int found_alice = 0, found_carol = 0, found_bob = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (out[i].id == 1) found_alice = 1;
        if (out[i].id == 3) found_carol = 1;
        if (out[i].id == 2) found_bob = 1;
    }
    BB_ASSERT(found_alice && found_carol && !found_bob);

    free(out);
}

static void test_apply_sorts(void)
{
    printf("\tTesting bb_query_apply sorts rows...\n");

    Person people[] = {
        make(1, "Carol", 40),
        make(2, "Alice", 30),
        make(3, "Bob", 17),
    };

    bb_query_t q;
    bb_query_init(&q, &person_schema);
    BB_ASSERT(bb_query_order_by(&q, "name", BB_ORDER_ASC) == 0);

    Person *out = NULL;
    size_t count = 0;
    BB_ASSERT(bb_query_apply(&q, people, 3, (void **)&out, &count) == 0);
    BB_ASSERT(count == 3);

    BB_ASSERT(strcmp(out[0].name, "Alice") == 0);
    BB_ASSERT(strcmp(out[1].name, "Bob") == 0);
    BB_ASSERT(strcmp(out[2].name, "Carol") == 0);

    free(out);

    /* descending by age */
    bb_query_t q2;
    bb_query_init(&q2, &person_schema);
    BB_ASSERT(bb_query_order_by(&q2, "age", BB_ORDER_DESC) == 0);

    Person *out2 = NULL;
    size_t count2 = 0;
    BB_ASSERT(bb_query_apply(&q2, people, 3, (void **)&out2, &count2) == 0);
    BB_ASSERT(count2 == 3);
    BB_ASSERT(out2[0].age == 40);
    BB_ASSERT(out2[1].age == 30);
    BB_ASSERT(out2[2].age == 17);

    free(out2);
}

static void test_apply_limit_offset(void)
{
    printf("\tTesting bb_query_apply applies limit/offset after sorting...\n");

    Person people[5];
    for (int i = 0; i < 5; i++)
        people[i] = make(i + 1, "x", (i + 1) * 10); /* ages: 10,20,30,40,50 */

    bb_query_t q;
    bb_query_init(&q, &person_schema);
    BB_ASSERT(bb_query_order_by(&q, "age", BB_ORDER_ASC) == 0);
    bb_query_limit(&q, 2);
    bb_query_offset(&q, 1);

    Person *out = NULL;
    size_t count = 0;
    BB_ASSERT(bb_query_apply(&q, people, 5, (void **)&out, &count) == 0);
    BB_ASSERT(count == 2);
    BB_ASSERT(out[0].age == 20); /* skipped the first (10) */
    BB_ASSERT(out[1].age == 30);

    free(out);
}

static void test_apply_empty_input(void)
{
    printf("\tTesting bb_query_apply with no input rows...\n");

    bb_query_t q;
    bb_query_init(&q, &person_schema);

    void *out = (void *)0x1; /* sentinel to make sure it gets set to NULL */
    size_t count = 999;
    BB_ASSERT(bb_query_apply(&q, NULL, 0, &out, &count) == 0);
    BB_ASSERT(out == NULL);
    BB_ASSERT(count == 0);
}

int main(void)
{
    printf("Running query builder/evaluator unit tests...\n");

    test_builder_basic();
    test_builder_rejects_unknown_field();
    test_builder_condition_capacity();

    test_matches_eq_and_multiple_conditions();
    test_matches_comparison_ops();
    test_matches_like();

    test_apply_filters();
    test_apply_sorts();
    test_apply_limit_offset();
    test_apply_empty_input();

    printf("All query tests passed!\n");
    return 0;
}
