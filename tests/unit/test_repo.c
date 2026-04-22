#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repo/repo.h"
#include "persist/model.h"
#include "persist/schema.h"

/* ---------------------------
 * Mock Backend State
 * --------------------------- */

typedef struct {
    int insert_called;
    int find_called;
    int update_called;
    int remove_called;

    int stored_id;
    char stored_name[64];
    int exists;
} MockState;

/* ---------------------------
 * Mock Backend
 * --------------------------- */

static BB_ModelHandle *mock_open(const char *uri)
{
    (void)uri;

    MockState *m = malloc(sizeof(MockState));
    memset(m, 0, sizeof(MockState));
    return (BB_ModelHandle *)m;
}

static void mock_close(BB_ModelHandle *h)
{
    free(h);
}

static int mock_insert(BB_ModelHandle *h, BB_Schema *schema, void *entity)
{
    MockState *m = (MockState *)h;
    m->insert_called++;

    int id = *(int *)((char *)entity + schema->fields[0].offset);
    char *name = (char *)entity + schema->fields[1].offset;

    if (m->exists && m->stored_id == id)
        return -1;

    m->stored_id = id;
    strncpy(m->stored_name, name, sizeof(m->stored_name));
    m->exists = 1;

    return 0;
}

static int mock_find(BB_ModelHandle *h, BB_Schema *schema, void *out, int id)
{
    MockState *m = (MockState *)h;
    m->find_called++;

    if (!m->exists || m->stored_id != id)
        return -1;

    *(int *)((char *)out + schema->fields[0].offset) = m->stored_id;
    snprintf((char *)out + schema->fields[1].offset,
             schema->fields[1].size,
             "%s", m->stored_name);

    return 0;
}

static int mock_update(BB_ModelHandle *h, BB_Schema *schema, void *entity)
{
    MockState *m = (MockState *)h;
    m->update_called++;

    int id = *(int *)((char *)entity + schema->fields[0].offset);
    char *name = (char *)entity + schema->fields[1].offset;

    if (!m->exists || m->stored_id != id)
        return -1;

    snprintf(m->stored_name, sizeof(m->stored_name), "%s", name);
    return 0;
}

static int mock_remove(BB_ModelHandle *h, BB_Schema *schema, int id)
{
    MockState *m = (MockState *)h;
    m->remove_called++;

    if (!m->exists || m->stored_id != id)
        return -1;

    m->exists = 0;
    return 0;
}

static BB_ModelAPI mock_api = {
    .name = "mock",
    .open = mock_open,
    .close = mock_close,
    .insert = mock_insert,
    .find_by_id = mock_find,
    .update = mock_update,
    .remove = mock_remove
};

/* ---------------------------
 * Test Model
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
 * Tests
 * --------------------------- */

static void test_repo_insert_and_find()
{
    printf("\tRepo insert & find...\n");

    BB_ModelHandle *h = mock_api.open("ignored");

    BB_Repo repo;
    bb_repo_init(&repo, &mock_api, h, &schema);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(bb_repo_insert(&repo, &u) == 0);

    User out = {0};
    assert(bb_repo_find_by_id(&repo, &out, 1) == 0);

    assert(out.id == 1);
    assert(strcmp(out.name, "Alice") == 0);

    mock_api.close(h);
}

static void test_repo_update()
{
    printf("\tRepo update...\n");

    BB_ModelHandle *h = mock_api.open("ignored");

    BB_Repo repo;
    bb_repo_init(&repo, &mock_api, h, &schema);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(bb_repo_insert(&repo, &u) == 0);

    strncpy(u.name, "Bob", sizeof(u.name));
    assert(bb_repo_update(&repo, &u) == 0);

    User out = {0};
    assert(bb_repo_find_by_id(&repo, &out, 1) == 0);

    assert(strcmp(out.name, "Bob") == 0);

    mock_api.close(h);
}

static void test_repo_remove()
{
    printf("\tRepo remove...\n");

    BB_ModelHandle *h = mock_api.open("ignored");

    BB_Repo repo;
    bb_repo_init(&repo, &mock_api, h, &schema);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(bb_repo_insert(&repo, &u) == 0);
    assert(bb_repo_remove(&repo, 1) == 0);

    User out = {0};
    assert(bb_repo_find_by_id(&repo, &out, 1) != 0);

    mock_api.close(h);
}

static void test_repo_error_propagation()
{
    printf("\tRepo error propagation...\n");

    BB_ModelHandle *h = mock_api.open("ignored");

    BB_Repo repo;
    bb_repo_init(&repo, &mock_api, h, &schema);

    User u1 = { .id = 1 };
    strncpy(u1.name, "Alice", sizeof(u1.name));

    User u2 = { .id = 1 };
    strncpy(u2.name, "Bob", sizeof(u2.name));

    assert(bb_repo_insert(&repo, &u1) == 0);

    /* duplicate insert should fail */
    assert(bb_repo_insert(&repo, &u2) != 0);

    mock_api.close(h);
}

/* ---------------------------
 * Main
 * --------------------------- */

int main(void)
{
    printf("Running repo unit tests...\n");

    test_repo_insert_and_find();
    test_repo_update();
    test_repo_remove();
    test_repo_error_propagation();

    printf("All repo tests passed!\n");
    return 0;
}