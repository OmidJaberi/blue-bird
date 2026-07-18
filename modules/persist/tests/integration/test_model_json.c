#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

#include "blue-bird/persist/model.h"
#include "blue-bird/persist/model/model_json.h"
#include "blue-bird/persist/schema.h"

/* ---------------------------
 * Test Model
 * --------------------------- */

typedef struct {
    int id;
    char name[64];
} User;

static bb_field_t user_fields[] = {
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
    }
};

static bb_schema_t user_schema = {
    .name = "users",
    .fields = user_fields,
    .field_count = 2,
    .struct_size = sizeof(User),
    .primary_key_index = 0
};

/* ---------------------------
 * Helpers
 * --------------------------- */

static void cleanup_file(const char *path)
{
    remove(path);
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_json_insert_and_find(void)
{
    printf("\tJSON insert & find...\n");

    const char *path = "test_model_json.json";
    cleanup_file(path);

    BB_ASSERT(bb_model_register(bb_model_json_api()) == 0);

    const bb_model_api_t *api = bb_model_get("json");
    BB_ASSERT(api);

    bb_model_handle_t *h = api->open(path);
    BB_ASSERT(h);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    BB_ASSERT(api->insert(h, &user_schema, &u) == 0);

    User out = {0};
    int id = 1;
    BB_ASSERT(api->find_by_pk(h, &user_schema, &out, &id) == 0);

    BB_ASSERT(out.id == 1);
    BB_ASSERT(strcmp(out.name, "Alice") == 0);

    api->close(h);
}

static void test_json_update(void)
{
    printf("\tJSON update...\n");

    const char *path = "test_model_json_update.json";
    cleanup_file(path);

    const bb_model_api_t *api = bb_model_get("json");
    bb_model_handle_t *h = api->open(path);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));
    BB_ASSERT(api->insert(h, &user_schema, &u) == 0);

    strncpy(u.name, "Bob", sizeof(u.name));
    BB_ASSERT(api->update(h, &user_schema, &u) == 0);

    User out = {0};
    int id = 1;
    BB_ASSERT(api->find_by_pk(h, &user_schema, &out, &id) == 0);

    BB_ASSERT(strcmp(out.name, "Bob") == 0);

    api->close(h);
}

static void test_json_remove(void)
{
    printf("\tJSON remove...\n");

    const char *path = "test_model_json_remove.json";
    cleanup_file(path);

    const bb_model_api_t *api = bb_model_get("json");
    bb_model_handle_t *h = api->open(path);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    BB_ASSERT(api->insert(h, &user_schema, &u) == 0);
    int id = 1;
    BB_ASSERT(api->remove(h, &user_schema, &id) == 0);

    User out = {0};
    id = 1;
    BB_ASSERT(api->find_by_pk(h, &user_schema, &out, &id) != 0);

    api->close(h);
}

static void test_json_conflict(void)
{
    printf("\tJSON insert conflict...\n");

    const char *path = "test_model_json_conflict.json";
    cleanup_file(path);

    const bb_model_api_t *api = bb_model_get("json");
    bb_model_handle_t *h = api->open(path);

    User u1 = { .id = 1 };
    strncpy(u1.name, "Alice", sizeof(u1.name));

    User u2 = { .id = 1 };
    strncpy(u2.name, "Bob", sizeof(u2.name));

    BB_ASSERT(api->insert(h, &user_schema, &u1) == 0);
    BB_ASSERT(api->insert(h, &user_schema, &u2) != 0);

    api->close(h);
}

int main(void)
{
    printf("Running JSON model integration tests...\n");

    test_json_insert_and_find();
    test_json_update();
    test_json_remove();
    test_json_conflict();

    printf("All JSON model tests passed!\n");
    return 0;
}
