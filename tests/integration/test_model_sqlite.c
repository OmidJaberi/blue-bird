#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "persist/model.h"
#include "persist/model/model_sqlite.h"
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
 * Test Helpers
 * --------------------------- */

static void cleanup_db(const char *path)
{
    remove(path); // delete file if exists
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_sqlite_insert_and_find()
{
    printf("\tTesting insert and find...\n");
    const char *db_path = "test_model_sqlite.db";
    cleanup_db(db_path);

    /* register backend */
    assert(bb_model_register(bb_model_sqlite_api()) == 0);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    /* insert */
    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    int rc = api->insert(h, &user_schema, &u);
    assert(rc == 0);

    /* fetch */
    User out;
    memset(&out, 0, sizeof(out));

    rc = api->find_by_id(h, &user_schema, &out, 1);
    assert(rc == 0);

    assert(out.id == 1);
    assert(strcmp(out.name, "Alice") == 0);

    api->close(h);
}

static void test_sqlite_multiple_inserts()
{
    printf("\tTesting multiple insert...\n");
    const char *db_path = "test_model_sqlite_multi.db";
    cleanup_db(db_path);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    User u1 = { .id = 1 };
    strncpy(u1.name, "Alice", sizeof(u1.name));

    User u2 = { .id = 2 };
    strncpy(u2.name, "Bob", sizeof(u2.name));

    assert(api->insert(h, &user_schema, &u1) == 0);
    assert(api->insert(h, &user_schema, &u2) == 0);

    User out1 = {0};
    User out2 = {0};

    assert(api->find_by_id(h, &user_schema, &out1, 1) == 0);
    assert(api->find_by_id(h, &user_schema, &out2, 2) == 0);

    assert(strcmp(out1.name, "Alice") == 0);
    assert(strcmp(out2.name, "Bob") == 0);

    api->close(h);
}

static void test_sqlite_not_found()
{
    printf("\tTesting not found...\n");
    const char *db_path = "test_model_sqlite_not_found.db";
    cleanup_db(db_path);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    User out = {0};

    int rc = api->find_by_id(h, &user_schema, &out, 999);
    assert(rc != 0); // should fail

    api->close(h);
}

static void test_sqlite_update()
{
    printf("\tTesting update...\n");
    const char *db_path = "test_model_sqlite_update.db";
    cleanup_db(db_path);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(api->insert(h, &user_schema, &u) == 0);

    // update
    strncpy(u.name, "Bob", sizeof(u.name));
    assert(api->update(h, &user_schema, &u) == 0);

    User out = {0};
    assert(api->find_by_id(h, &user_schema, &out, 1) == 0);

    assert(strcmp(out.name, "Bob") == 0);

    api->close(h);
}

static void test_sqlite_remove()
{
    printf("\tTesting remove...\n");
    const char *db_path = "test_model_sqlite_remove.db";
    cleanup_db(db_path);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(api->insert(h, &user_schema, &u) == 0);

    // remove
    assert(api->remove(h, &user_schema, 1) == 0);

    User out = {0};
    int rc = api->find_by_id(h, &user_schema, &out, 1);

    assert(rc != 0); // should not exist

    api->close(h);
}

static void test_sqlite_insert_conflict()
{
    printf("\tTesting insert conflict...\n");
    const char *db_path = "test_model_sqlite_conflict.db";
    cleanup_db(db_path);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    User u1 = { .id = 1 };
    strncpy(u1.name, "Alice", sizeof(u1.name));

    User u2 = { .id = 1 };
    strncpy(u2.name, "Bob", sizeof(u2.name));

    assert(api->insert(h, &user_schema, &u1) == 0);

    // second insert with same PK should fail
    int rc = api->insert(h, &user_schema, &u2);
    assert(rc != 0);

    api->close(h);
}

static void test_sqlite_update_not_found()
{
    printf("\tTesting update on non-existent row...\n");
    const char *db_path = "test_model_sqlite_update_not_found.db";
    cleanup_db(db_path);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    User u = { .id = 999 };
    strncpy(u.name, "Ghost", sizeof(u.name));

    int rc = api->update(h, &user_schema, &u);

    assert(rc != 0); // should fail

    api->close(h);
}

static void test_sqlite_remove_not_found()
{
    printf("\tTesting remove on non-existent row...\n");
    const char *db_path = "test_model_sqlite_remove_not_found.db";
    cleanup_db(db_path);

    const BB_ModelAPI *api = bb_model_get("sqlite");
    assert(api != NULL);

    BB_ModelHandle *h = api->open(db_path);
    assert(h != NULL);

    int rc = api->remove(h, &user_schema, 999);

    assert(rc != 0); // should fail

    api->close(h);
}

int main(void)
{
    printf("Running SQLite model integration tests...\n");

    test_sqlite_insert_and_find();
    test_sqlite_multiple_inserts();
    test_sqlite_not_found();
    test_sqlite_update();
    test_sqlite_remove();
    test_sqlite_insert_conflict();
    test_sqlite_update_not_found();
    test_sqlite_remove_not_found();

    printf("All SQLite model tests passed!\n");
    return 0;
}