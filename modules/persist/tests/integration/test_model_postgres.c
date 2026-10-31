#include <blue-bird/error/assert.h>
#include <pthread.h>
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
 * e.g. postgresql://postgres:postgres@localhost:5432/postgres
 *
 * Every row the test inserts is removed again; the (empty) table
 * bb_test_people_pg is left behind. */

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

/* Fresh uuid, and a name unique to this run so lookups by name are exact. */
static void make_person(Person *p, int age, const char *label)
{
    memset(p, 0, sizeof(*p));
    BB_ASSERT(bb_uuid_v4_string(p->id) == 0);
    p->age = age;
    snprintf(p->name, sizeof(p->name), "%s-%s", label, p->id);
}

static const Person *find_in(const Person *all, size_t count, const char *id)
{
    for (size_t i = 0; i < count; i++)
        if (strcmp(all[i].id, id) == 0)
            return &all[i];
    return NULL;
}

static void test_postgres_crud(const char *uri)
{
    printf("\tTesting insert / find / update / find_all / remove (uuid pk)...\n");

    BB_ASSERT(bb_model_register(bb_model_postgres_api()) == 0);
    const bb_model_api_t *api = bb_model_get("postgres");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(uri);
    BB_ASSERT(h != NULL);

    Person alice, bob;
    make_person(&alice, 42, "alice");
    make_person(&bob, 7, "bob");

    /* insert + duplicate pk */
    BB_ASSERT(api->insert(h, &person_schema, &alice) == 0);
    BB_ASSERT(api->insert(h, &person_schema, &bob) == 0);
    BB_ASSERT(api->insert(h, &person_schema, &alice) != 0);

    /* find_by_pk */
    Person out;
    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_by_pk(h, &person_schema, &out, alice.id) == 0);
    BB_ASSERT(strcmp(out.id, alice.id) == 0);
    BB_ASSERT(out.age == 42);
    BB_ASSERT(strcmp(out.name, alice.name) == 0);

    bb_uuid_t unknown;
    BB_ASSERT(bb_uuid_v4_string(unknown) == 0);
    BB_ASSERT(api->find_by_pk(h, &person_schema, &out, unknown) != 0);

    /* find_first_by_field: string and int fields, miss, bad field */
    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_first_by_field(h, &person_schema, &out, "name", bob.name) == 0);
    BB_ASSERT(strcmp(out.id, bob.id) == 0);
    BB_ASSERT(out.age == 7);

    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_first_by_field(h, &person_schema, &out, "id", alice.id) == 0);
    BB_ASSERT(strcmp(out.name, alice.name) == 0);

    BB_ASSERT(api->find_first_by_field(h, &person_schema, &out, "name", "no-such-name") != 0);
    BB_ASSERT(api->find_first_by_field(h, &person_schema, &out, "nope", alice.name) != 0);

    /* update: changes the row, leaves the other one alone */
    alice.age = 43;
    snprintf(alice.name, sizeof(alice.name), "alice2-%s", alice.id);
    BB_ASSERT(api->update(h, &person_schema, &alice) == 0);

    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_by_pk(h, &person_schema, &out, alice.id) == 0);
    BB_ASSERT(out.age == 43);
    BB_ASSERT(strcmp(out.name, alice.name) == 0);

    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_by_pk(h, &person_schema, &out, bob.id) == 0);
    BB_ASSERT(out.age == 7);

    /* update of a row that does not exist */
    Person ghost;
    make_person(&ghost, 1, "ghost");
    BB_ASSERT(api->update(h, &person_schema, &ghost) != 0);

    /* find_all contains both rows with the right contents */
    Person *all = NULL;
    size_t count = 0;
    BB_ASSERT(api->find_all(h, &person_schema, (void **)&all, &count) == 0);
    BB_ASSERT(all != NULL);
    BB_ASSERT(count >= 2);

    const Person *fa = find_in(all, count, alice.id);
    const Person *fb = find_in(all, count, bob.id);
    BB_ASSERT(fa != NULL && fb != NULL);
    BB_ASSERT(fa->age == 43 && strcmp(fa->name, alice.name) == 0);
    BB_ASSERT(fb->age == 7 && strcmp(fb->name, bob.name) == 0);
    free(all);

    /* remove: first time works, second time (no row) fails */
    BB_ASSERT(api->remove(h, &person_schema, alice.id) == 0);
    BB_ASSERT(api->remove(h, &person_schema, alice.id) != 0);
    BB_ASSERT(api->find_by_pk(h, &person_schema, &out, alice.id) != 0);

    BB_ASSERT(api->remove(h, &person_schema, bob.id) == 0);

    /* find_all on a table with our rows gone still succeeds */
    all = NULL;
    count = 0;
    BB_ASSERT(api->find_all(h, &person_schema, (void **)&all, &count) == 0);
    BB_ASSERT(all != NULL);
    BB_ASSERT(find_in(all, count, alice.id) == NULL);
    BB_ASSERT(find_in(all, count, bob.id) == NULL);
    free(all);

    api->close(h);
}

/* ---------------------------
 * Concurrency: one shared handle, more threads than pool connections.
 * Each thread works on its own rows (insert -> find -> update -> remove),
 * so any failure means the pool handed out a connection twice, lost a
 * statement, or tripped over concurrent CREATE TABLE.
 * --------------------------- */

#define CONC_THREADS 16
#define CONC_OPS_PER_THREAD 20

typedef struct {
    const bb_model_api_t *api;
    bb_model_handle_t *h;
    int failed;
} conc_arg_t;

static void *conc_worker(void *arg_)
{
    conc_arg_t *arg = (conc_arg_t *)arg_;
    arg->failed = 0;

    for (int i = 0; i < CONC_OPS_PER_THREAD; i++)
    {
        Person p;
        make_person(&p, i, "conc");

        if (arg->api->insert(arg->h, &person_schema, &p) != 0) { arg->failed = 1; return NULL; }

        Person out;
        memset(&out, 0, sizeof(out));
        if (arg->api->find_by_pk(arg->h, &person_schema, &out, p.id) != 0 ||
            out.age != i || strcmp(out.name, p.name) != 0) { arg->failed = 1; return NULL; }

        p.age = i + 1000;
        if (arg->api->update(arg->h, &person_schema, &p) != 0) { arg->failed = 1; return NULL; }

        memset(&out, 0, sizeof(out));
        if (arg->api->find_first_by_field(arg->h, &person_schema, &out, "name", p.name) != 0 ||
            out.age != i + 1000) { arg->failed = 1; return NULL; }

        if (arg->api->remove(arg->h, &person_schema, p.id) != 0) { arg->failed = 1; return NULL; }
    }

    return NULL;
}

static void test_postgres_concurrent_access(const char *uri)
{
    printf("\tTesting concurrent access on a shared handle...\n");

    const bb_model_api_t *api = bb_model_get("postgres");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(uri);
    BB_ASSERT(h != NULL);

    pthread_t threads[CONC_THREADS];
    conc_arg_t args[CONC_THREADS];

    for (int i = 0; i < CONC_THREADS; i++)
    {
        args[i].api = api;
        args[i].h = h;
        args[i].failed = 0;
        BB_ASSERT(pthread_create(&threads[i], NULL, conc_worker, &args[i]) == 0);
    }

    int any_failed = 0;
    for (int i = 0; i < CONC_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
        any_failed |= args[i].failed;
    }
    BB_ASSERT(!any_failed);

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
    test_postgres_crud(uri);
    test_postgres_concurrent_access(uri);
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
