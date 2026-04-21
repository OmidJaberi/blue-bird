#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "persist/model.h"
#include "persist/schema.h"

/* ---------------------------
 * Mock State
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

static MockState mock;

/* ---------------------------
 * Mock Backend
 * --------------------------- */

static BB_ModelHandle *mock_open(const char *uri)
{
    (void)uri;
    memset(&mock, 0, sizeof(mock));
    return (BB_ModelHandle *)&mock;
}

static void mock_close(BB_ModelHandle *h)
{
    (void)h;
}

static int mock_insert(BB_ModelHandle *h, BB_Schema *schema, void *entity)
{
    (void)schema;
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
 * Tests
 * --------------------------- */

static void test_mock_backend()
{
    printf("\tTesting mock backend...\n");

    assert(bb_model_register(&mock_api) == 0);

    const BB_ModelAPI *api = bb_model_get("mock");
    assert(api);

    BB_ModelHandle *h = api->open("ignored");

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(api->insert(h, &schema, &u) == 0);
    assert(mock.insert_called == 1);

    User out = {0};
    assert(api->find_by_id(h, &schema, &out, 1) == 0);
    assert(mock.find_called == 1);

    strncpy(u.name, "Bob", sizeof(u.name));
    assert(api->update(h, &schema, &u) == 0);
    assert(mock.update_called == 1);

    assert(api->remove(h, &schema, 1) == 0);
    assert(mock.remove_called == 1);

    api->close(h);
}

int main(void)
{
    printf("Running mock model tests...\n");

    test_mock_backend();

    printf("All mock tests passed!\n");
    return 0;
}