#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "repo/repo.h"
#include "persist/model.h"
#include "persist/model/model_json.h"
#include "persist/schema.h"

/* ---------------------------
 * Model
 * --------------------------- */

typedef struct {
    int id;
    char name[64];
} User;

static BB_Field fields[] = {
    { "id", BB_FIELD_INT, offsetof(User, id), sizeof(int) },
    { "name", BB_FIELD_STRING, offsetof(User, name), 64 }
};

static BB_Schema schema = {
    .name = "users",
    .fields = fields,
    .field_count = 2,
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

static void test_repo_json_crud()
{
    printf("\tJSON repo CRUD...\n");

    const char *file = "test_repo_json.json";
    cleanup(file);

    assert(bb_model_register(bb_model_json_api()) == 0);

    const BB_ModelAPI *api = bb_model_get("json");
    assert(api);

    BB_ModelHandle *h = api->open(file);
    assert(h);

    BB_Repo repo;
    bb_repo_init(&repo, api, h, &schema);

    /* INSERT */
    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));
    assert(bb_repo_insert(&repo, &u) == 0);

    /* FIND */
    User out = {0};
    assert(bb_repo_find_by_id(&repo, &out, 1) == 0);
    assert(strcmp(out.name, "Alice") == 0);

    /* UPDATE */
    strncpy(u.name, "Bob", sizeof(u.name));
    assert(bb_repo_update(&repo, &u) == 0);

    memset(&out, 0, sizeof(out));
    assert(bb_repo_find_by_id(&repo, &out, 1) == 0);
    assert(strcmp(out.name, "Bob") == 0);

    /* REMOVE */
    assert(bb_repo_remove(&repo, 1) == 0);
    assert(bb_repo_find_by_id(&repo, &out, 1) != 0);

    api->close(h);
}

int main(void)
{
    printf("Running JSON repo integration tests...\n");

    test_repo_json_crud();

    printf("All JSON repo tests passed!\n");
    return 0;
}