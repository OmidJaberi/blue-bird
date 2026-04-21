#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "persist/model.h"
#include "persist/model/model_json.h"
#include "persist/schema.h"

/* ---------------------------
 * Test Model
 * --------------------------- */

typedef struct {
    int id;
    char name[64];
} User;

static BB_Field user_fields[] = {
    { "id", BB_FIELD_INT, offsetof(User, id), sizeof(int) },
    { "name", BB_FIELD_STRING, offsetof(User, name), 64 }
};

static BB_Schema user_schema = {
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

static void test_json_insert_and_find()
{
    printf("\tJSON insert & find...\n");

    const char *path = "test_model_json.json";
    cleanup_file(path);

    assert(bb_model_register(bb_model_json_api()) == 0);

    const BB_ModelAPI *api = bb_model_get("json");
    assert(api);

    BB_ModelHandle *h = api->open(path);
    assert(h);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(api->insert(h, &user_schema, &u) == 0);

    User out = {0};
    assert(api->find_by_id(h, &user_schema, &out, 1) == 0);

    assert(out.id == 1);
    assert(strcmp(out.name, "Alice") == 0);

    api->close(h);
}

static void test_json_update()
{
    printf("\tJSON update...\n");

    const char *path = "test_model_json_update.json";
    cleanup_file(path);

    const BB_ModelAPI *api = bb_model_get("json");
    BB_ModelHandle *h = api->open(path);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));
    assert(api->insert(h, &user_schema, &u) == 0);

    strncpy(u.name, "Bob", sizeof(u.name));
    assert(api->update(h, &user_schema, &u) == 0);

    User out = {0};
    assert(api->find_by_id(h, &user_schema, &out, 1) == 0);

    assert(strcmp(out.name, "Bob") == 0);

    api->close(h);
}

static void test_json_remove()
{
    printf("\tJSON remove...\n");

    const char *path = "test_model_json_remove.json";
    cleanup_file(path);

    const BB_ModelAPI *api = bb_model_get("json");
    BB_ModelHandle *h = api->open(path);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(api->insert(h, &user_schema, &u) == 0);
    assert(api->remove(h, &user_schema, 1) == 0);

    User out = {0};
    assert(api->find_by_id(h, &user_schema, &out, 1) != 0);

    api->close(h);
}

static void test_json_conflict()
{
    printf("\tJSON insert conflict...\n");

    const char *path = "test_model_json_conflict.json";
    cleanup_file(path);

    const BB_ModelAPI *api = bb_model_get("json");
    BB_ModelHandle *h = api->open(path);

    User u1 = { .id = 1 };
    strncpy(u1.name, "Alice", sizeof(u1.name));

    User u2 = { .id = 1 };
    strncpy(u2.name, "Bob", sizeof(u2.name));

    assert(api->insert(h, &user_schema, &u1) == 0);
    assert(api->insert(h, &user_schema, &u2) != 0);

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