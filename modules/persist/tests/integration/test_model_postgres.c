#include <blue-bird/error/assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blue-bird/persist/model.h"
#include "blue-bird/persist/schema.h"
#include "blue-bird/utils/uuid.h"

#ifdef BB_WITH_POSTGRES

#include "blue-bird/persist/model/model_postgres.h"

/* Runs only when BB_TEST_POSTGRES_URI points at a disposable database,
 * e.g. postgresql://postgres:postgres@localhost:5432/postgres */

typedef struct {
    bb_uuid_t id;
    int age;
    char name[64];
} Person;

static bb_field_t person_fields[] = {
    { .name = "id",   .type = BB_FIELD_UUID,   .offset = offsetof(Person, id),   .size = sizeof(bb_uuid_t), .flags = BB_FIELD_PRIMARY_KEY },
    { .name = "age",  .type = BB_FIELD_INT,    .offset = offsetof(Person, age),  .size = sizeof(int),       .flags = BB_FIELD_NONE },
    { .name = "name", .type = BB_FIELD_STRING, .offset = offsetof(Person, name), .size = 64,                .flags = BB_FIELD_NONE }
};

static bb_schema_t person_schema = {
    .name = "bb_test_people_pg",
    .fields = person_fields,
    .field_count = 3,
    .struct_size = sizeof(Person),
    .primary_key_index = 0
};

static void test_postgres_insert_and_find(const char *uri)
{
    printf("\tTesting insert and find (uuid pk)...\n");

    BB_ASSERT(bb_model_register(bb_model_postgres_api()) == 0);
    const bb_model_api_t *api = bb_model_get("postgres");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(uri);
    BB_ASSERT(h != NULL);

    Person p;
    memset(&p, 0, sizeof(p));
    BB_ASSERT(bb_uuid_v4_string(p.id) == 0);
    p.age = 42;
    strncpy(p.name, "Alice", sizeof(p.name) - 1);

    BB_ASSERT(api->insert(h, &person_schema, &p) == 0);

    /* duplicate primary key must fail */
    BB_ASSERT(api->insert(h, &person_schema, &p) != 0);

    Person out;
    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_by_pk(h, &person_schema, &out, p.id) == 0);
    BB_ASSERT(strcmp(out.id, p.id) == 0);
    BB_ASSERT(out.age == 42);
    BB_ASSERT(strcmp(out.name, "Alice") == 0);

    /* unknown key */
    bb_uuid_t other;
    BB_ASSERT(bb_uuid_v4_string(other) == 0);
    BB_ASSERT(api->find_by_pk(h, &person_schema, &out, other) != 0);

    api->close(h);
}

int main(void)
{
    const char *uri = getenv("BB_TEST_POSTGRES_URI");
    if (!uri || !*uri)
    {
        printf("Skipping postgres model tests (BB_TEST_POSTGRES_URI not set)\n");
        return 0;
    }

    printf("Running postgres model tests...\n");
    test_postgres_insert_and_find(uri);
    printf("All postgres model tests passed\n");
    return 0;
}

#else

int main(void)
{
    printf("Skipping postgres model tests (built without BB_WITH_POSTGRES)\n");
    return 0;
}

#endif
