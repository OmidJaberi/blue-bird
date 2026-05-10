#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blue-bird/persist/repo.h"
#include "blue-bird/persist/model.h"
#include "blue-bird/persist/schema.h"

/* ---------------------------
 * Test Model
 * --------------------------- */

typedef struct {
    int id;
    char name[64];
} User;

static BB_Field fields[] = {
    { "id", BB_FIELD_INT, offsetof(User, id), sizeof(int), NULL, NULL },
    { "name", BB_FIELD_STRING, offsetof(User, name), 64, NULL, NULL }
};

static BB_Schema schema = {
    .name = "users",
    .fields = fields,
    .field_count = 2,
    .struct_size = sizeof(User),
    .primary_key_index = 0
};

/* ---------------------------
 * Mock Backend State
 * --------------------------- */

typedef struct {
    int insert_called;
    int find_called;
    int update_called;
    int remove_called;

    User users[16];
    size_t count;
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
    (void)schema;

    MockState *m = (MockState *)h;

    m->insert_called++;

    User *u = entity;

    for (size_t i = 0; i < m->count; i++)
    {
        if (m->users[i].id == u->id)
            return -1;
    }

    if (m->count >= 16)
        return -1;

    m->users[m->count++] = *u;

    return 0;
}

static int mock_find_by_pk(BB_ModelHandle *h, BB_Schema *schema, void *out, const void *key)
{
    (void)schema;

    MockState *m = (MockState *)h;

    m->find_called++;

    int id = *(const int *)key;

    for (size_t i = 0; i < m->count; i++)
    {
        if (m->users[i].id == id)
        {
            *(User *)out = m->users[i];
            return 0;
        }
    }

    return -1;
}

static int mock_update(BB_ModelHandle *h, BB_Schema *schema, void *entity)
{
    (void)schema;

    MockState *m = (MockState *)h;

    m->update_called++;

    User *u = entity;

    for (size_t i = 0; i < m->count; i++)
    {
        if (m->users[i].id == u->id)
        {
            m->users[i] = *u;
            return 0;
        }
    }

    return -1;
}

static int mock_remove(BB_ModelHandle *h, BB_Schema *schema, const void *key)
{
    (void)schema;

    MockState *m = (MockState *)h;

    m->remove_called++;

    int id = *(const int *)key;

    for (size_t i = 0; i < m->count; i++)
    {
        if (m->users[i].id == id)
        {
            for (size_t j = i;
                 j < m->count - 1;
                 j++)
            {
                m->users[j] =
                    m->users[j + 1];
            }

            m->count--;

            return 0;
        }
    }

    return -1;
}

static int mock_find_all(BB_ModelHandle *h, BB_Schema *schema, void **out, size_t *count)
{
    MockState *m = (MockState *)h;

    *count = m->count;

    if (m->count == 0)
    {
        *out = NULL;
        return 0;
    }

    void *buf =
        calloc(m->count,
               schema->struct_size);

    if (!buf)
        return -1;

    memcpy(
        buf,
        m->users,
        m->count * sizeof(User)
    );

    *out = buf;

    return 0;
}

static BB_ModelAPI mock_api = {
    .name = "mock",
    .open = mock_open,
    .close = mock_close,
    .insert = mock_insert,
    .find_by_pk = mock_find_by_pk,
    .update = mock_update,
    .remove = mock_remove,
    .find_all = mock_find_all
};

/* ---------------------------
 * Filter
 * --------------------------- */

static int filter_even_ids(const void *entity, void *ctx)
{
    (void)ctx;

    const User *u = entity;

    return (u->id % 2) == 0;
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_repo_insert_and_find(void)
{
    printf("\tRepo insert & find...\n");

    BB_ModelHandle *h = mock_api.open("ignored");

    BB_Repo repo;
    bb_repo_init(&repo, &mock_api, h, &schema);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(bb_repo_insert(&repo, &u) == 0);

    User out = {0};
    assert(bb_repo_find_by_pk(&repo, &out, &(int){1}) == 0);

    assert(out.id == 1);
    assert(strcmp(out.name, "Alice") == 0);

    mock_api.close(h);
}

static void test_repo_update(void)
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
    assert(bb_repo_find_by_pk(&repo, &out, &(int){1}) == 0);

    assert(strcmp(out.name, "Bob") == 0);

    mock_api.close(h);
}

static void test_repo_remove(void)
{
    printf("\tRepo remove...\n");

    BB_ModelHandle *h = mock_api.open("ignored");

    BB_Repo repo;
    bb_repo_init(&repo, &mock_api, h, &schema);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    assert(bb_repo_insert(&repo, &u) == 0);
    assert(bb_repo_remove(&repo, &(int){1}) == 0);

    User out = {0};
    assert(bb_repo_find_by_pk(&repo, &out, &(int){1}) != 0);

    mock_api.close(h);
}

static void test_repo_error_propagation(void)
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

static void test_repo_filter(void)
{
    printf("\tRepo filter...\n");

    BB_ModelHandle *h = mock_api.open("ignored");

    BB_Repo repo;

    bb_repo_init(
        &repo,
        &mock_api,
        h,
        &schema
    );

    User u1 = { .id = 1 };
    strcpy(u1.name, "Alice");

    User u2 = { .id = 2 };
    strcpy(u2.name, "Bob");

    User u3 = { .id = 4 };
    strcpy(u3.name, "Charlie");

    assert(bb_repo_insert(&repo, &u1) == 0);
    assert(bb_repo_insert(&repo, &u2) == 0);
    assert(bb_repo_insert(&repo, &u3) == 0);

    User *filtered = NULL;
    size_t count = 0;

    assert(
        bb_repo_filter(
            &repo,
            (void **)&filtered,
            &count,
            filter_even_ids,
            NULL
        ) == 0
    );

    assert(count == 2);

    assert(filtered[0].id == 2);
    assert(filtered[1].id == 4);

    free(filtered);

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
    test_repo_filter();

    printf("All repo tests passed!\n");
    return 0;
}
