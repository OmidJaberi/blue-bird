#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "blue-bird/persist/model.h"
#include "blue-bird/persist/model/model_sqlite.h"
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
 * Test Helpers
 * --------------------------- */

static void cleanup_db(const char *path)
{
    remove(path); // delete file if exists
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_sqlite_insert_and_find(void)
{
    printf("\tTesting insert and find...\n");
    const char *db_path = "test_model_sqlite.db";
    cleanup_db(db_path);

    /* register backend */
    BB_ASSERT(bb_model_register(bb_model_sqlite_api()) == 0);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    /* insert */
    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    int rc = api->insert(h, &user_schema, &u);
    BB_ASSERT(rc == 0);

    /* fetch */
    User out;
    memset(&out, 0, sizeof(out));

    int id = 1;
    rc = api->find_by_pk(h, &user_schema, &out, &id);
    BB_ASSERT(rc == 0);

    BB_ASSERT(out.id == 1);
    BB_ASSERT(strcmp(out.name, "Alice") == 0);

    api->close(h);
}

static void test_sqlite_multiple_inserts(void)
{
    printf("\tTesting multiple insert...\n");
    const char *db_path = "test_model_sqlite_multi.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    User u1 = { .id = 1 };
    strncpy(u1.name, "Alice", sizeof(u1.name));

    User u2 = { .id = 2 };
    strncpy(u2.name, "Bob", sizeof(u2.name));

    BB_ASSERT(api->insert(h, &user_schema, &u1) == 0);
    BB_ASSERT(api->insert(h, &user_schema, &u2) == 0);

    User out1 = {0};
    User out2 = {0};

    int id = 1;
    BB_ASSERT(api->find_by_pk(h, &user_schema, &out1, &id) == 0);
    id = 2;
    BB_ASSERT(api->find_by_pk(h, &user_schema, &out2, &id) == 0);

    BB_ASSERT(strcmp(out1.name, "Alice") == 0);
    BB_ASSERT(strcmp(out2.name, "Bob") == 0);

    api->close(h);
}

static void test_sqlite_not_found(void)
{
    printf("\tTesting not found...\n");
    const char *db_path = "test_model_sqlite_not_found.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    User out = {0};

    int id = 999;
    int rc = api->find_by_pk(h, &user_schema, &out, &id);
    BB_ASSERT(rc != 0); // should fail

    api->close(h);
}

static void test_sqlite_update(void)
{
    printf("\tTesting update...\n");
    const char *db_path = "test_model_sqlite_update.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    BB_ASSERT(api->insert(h, &user_schema, &u) == 0);

    // update
    strncpy(u.name, "Bob", sizeof(u.name));
    BB_ASSERT(api->update(h, &user_schema, &u) == 0);

    User out = {0};
    int id = 1;
    BB_ASSERT(api->find_by_pk(h, &user_schema, &out, &id) == 0);

    BB_ASSERT(strcmp(out.name, "Bob") == 0);

    api->close(h);
}

static void test_sqlite_remove(void)
{
    printf("\tTesting remove...\n");
    const char *db_path = "test_model_sqlite_remove.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    BB_ASSERT(api->insert(h, &user_schema, &u) == 0);

    // remove
    int id = 1;
    BB_ASSERT(api->remove(h, &user_schema, &id) == 0);

    User out = {0};
    id = 1;
    int rc = api->find_by_pk(h, &user_schema, &out, &id);

    BB_ASSERT(rc != 0); // should not exist

    api->close(h);
}

static void test_sqlite_insert_conflict(void)
{
    printf("\tTesting insert conflict...\n");
    const char *db_path = "test_model_sqlite_conflict.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    User u1 = { .id = 1 };
    strncpy(u1.name, "Alice", sizeof(u1.name));

    User u2 = { .id = 1 };
    strncpy(u2.name, "Bob", sizeof(u2.name));

    BB_ASSERT(api->insert(h, &user_schema, &u1) == 0);

    // second insert with same PK should fail
    int rc = api->insert(h, &user_schema, &u2);
    BB_ASSERT(rc != 0);

    api->close(h);
}

static void test_sqlite_update_not_found(void)
{
    printf("\tTesting update on non-existent row...\n");
    const char *db_path = "test_model_sqlite_update_not_found.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    User u = { .id = 999 };
    strncpy(u.name, "Ghost", sizeof(u.name));

    int rc = api->update(h, &user_schema, &u);

    BB_ASSERT(rc != 0); // should fail

    api->close(h);
}

static void test_sqlite_remove_not_found(void)
{
    printf("\tTesting remove on non-existent row...\n");
    const char *db_path = "test_model_sqlite_remove_not_found.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    int id = 999;
    int rc = api->remove(h, &user_schema, &id);

    BB_ASSERT(rc != 0); // should fail

    api->close(h);
}

#define BB_LONG_NAME_LEN 2000

static void test_sqlite_long_table_name_no_overflow(void)
{
    printf("\tTesting long table name does not overflow SQL buffer...\n");

    /* Build a table name far longer than the old fixed 1024-byte
     * buffers used for CREATE TABLE / INSERT / UPDATE statements. */
    char long_name[BB_LONG_NAME_LEN + 1];
    memset(long_name, 'a', BB_LONG_NAME_LEN);
    long_name[BB_LONG_NAME_LEN] = '\0';

    bb_schema_t long_name_schema = {
        .name = long_name,
        .fields = user_fields,
        .field_count = 2,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    const char *db_path = "test_model_sqlite_long_name.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    User u = { .id = 1 };
    strncpy(u.name, "Alice", sizeof(u.name));

    /* CREATE TABLE + INSERT: this is where the old strcat-based
     * ensure_table()/sqlite_insert() would overrun their stack buffers. */
    BB_ASSERT(api->insert(h, &long_name_schema, &u) == 0);

    User out = {0};
    int id = 1;
    BB_ASSERT(api->find_by_pk(h, &long_name_schema, &out, &id) == 0);
    BB_ASSERT(out.id == 1);
    BB_ASSERT(strcmp(out.name, "Alice") == 0);

    strncpy(u.name, "Bob", sizeof(u.name));
    BB_ASSERT(api->update(h, &long_name_schema, &u) == 0);

    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_by_pk(h, &long_name_schema, &out, &id) == 0);
    BB_ASSERT(strcmp(out.name, "Bob") == 0);

    BB_ASSERT(api->remove(h, &long_name_schema, &id) == 0);

    memset(&out, 0, sizeof(out));
    BB_ASSERT(api->find_by_pk(h, &long_name_schema, &out, &id) != 0);

    api->close(h);
}

#define BB_WIDE_FIELD_COUNT 40

typedef struct {
    int id;
    int values[BB_WIDE_FIELD_COUNT];
} WideEntity;

static char wide_field_names[BB_WIDE_FIELD_COUNT][40];
static bb_field_t wide_fields[BB_WIDE_FIELD_COUNT + 1];

static void build_wide_schema(bb_schema_t *out_schema)
{
    wide_fields[0] = (bb_field_t){
        .name = "id",
        .type = BB_FIELD_INT,
        .offset = offsetof(WideEntity, id),
        .size = sizeof(int),
        .flags = BB_FIELD_NONE
    };

    for (int i = 0; i < BB_WIDE_FIELD_COUNT; i++)
    {
        /* Long-ish but individually unremarkable field names; it's the
         * accumulation across all of them that used to blow the
         * 1024-byte stack buffer. */
        snprintf(wide_field_names[i], sizeof(wide_field_names[i]),
                 "a_reasonably_long_field_name_%02d", i);

        wide_fields[i + 1] = (bb_field_t){
            .name = wide_field_names[i],
            .type = BB_FIELD_INT,
            .offset = offsetof(WideEntity, values) + i * sizeof(int),
            .size = sizeof(int),
            .flags = BB_FIELD_NONE
        };
    }

    *out_schema = (bb_schema_t){
        .name = "wide_entities",
        .fields = wide_fields,
        .field_count = BB_WIDE_FIELD_COUNT + 1,
        .struct_size = sizeof(WideEntity),
        .primary_key_index = 0
    };
}

static void test_sqlite_many_fields_no_overflow(void)
{
    printf("\tTesting schema with many fields does not overflow SQL buffer...\n");

    bb_schema_t wide_schema;
    build_wide_schema(&wide_schema);

    /* Sanity check: this schema alone comfortably exceeds the old
     * 1024-byte fixed SQL buffers once column names/types are laid out. */
    size_t approx_column_list_len = 0;
    for (size_t i = 0; i < wide_schema.field_count; i++)
        approx_column_list_len += strlen(wide_schema.fields[i].name) + strlen(", INTEGER");
    BB_ASSERT(approx_column_list_len > 1024);

    const char *db_path = "test_model_sqlite_wide.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    WideEntity e = { .id = 1 };
    for (int i = 0; i < BB_WIDE_FIELD_COUNT; i++)
        e.values[i] = i;

    BB_ASSERT(api->insert(h, &wide_schema, &e) == 0);

    void *rows = NULL;
    size_t row_count = 0;
    BB_ASSERT(api->find_all(h, &wide_schema, &rows, &row_count) == 0);
    BB_ASSERT(row_count == 1);

    WideEntity *out = (WideEntity *)rows;
    BB_ASSERT(out->id == 1);
    BB_ASSERT(out->values[BB_WIDE_FIELD_COUNT - 1] == BB_WIDE_FIELD_COUNT - 1);

    free(rows);
    api->close(h);
}

/* ---------------------------
 * Connection pool / concurrency tests
 *
 * These exercise the pooled backend from multiple threads sharing a
 * single bb_model_handle_t, which is exactly the case a single
 * sqlite3* handle couldn't support safely.
 * --------------------------- */

#define BB_POOL_TEST_THREADS 16
#define BB_POOL_TEST_OPS_PER_THREAD 50

typedef struct {
    const bb_model_api_t *api;
    bb_model_handle_t *h;
    int thread_idx;
    int failed; /* set to 1 by the thread on any assertion-style failure */
} bb_pool_worker_arg_t;

static void *bb_pool_worker(void *arg_)
{
    bb_pool_worker_arg_t *arg = (bb_pool_worker_arg_t *)arg_;
    arg->failed = 0;

    for (int i = 0; i < BB_POOL_TEST_OPS_PER_THREAD; i++)
    {
        int id = arg->thread_idx * BB_POOL_TEST_OPS_PER_THREAD + i;

        User u = { .id = id };
        snprintf(u.name, sizeof(u.name), "user-%d", id);

        if (arg->api->insert(arg->h, &user_schema, &u) != 0)
        {
            arg->failed = 1;
            return NULL;
        }

        User out = { 0 };
        if (arg->api->find_by_pk(arg->h, &user_schema, &out, &id) != 0)
        {
            arg->failed = 1;
            return NULL;
        }

        if (out.id != id || strcmp(out.name, u.name) != 0)
        {
            arg->failed = 1;
            return NULL;
        }

        /* Also drive find_all concurrently so readers and writers on
         * different pooled connections overlap, not just inserts. */
        void *rows = NULL;
        size_t count = 0;
        if (arg->api->find_all(arg->h, &user_schema, &rows, &count) != 0)
        {
            arg->failed = 1;
            return NULL;
        }
        free(rows);
    }

    return NULL;
}

static void test_sqlite_concurrent_access_shared_handle(void)
{
    printf("\tTesting concurrent access on a shared handle...\n");
    const char *db_path = "test_model_sqlite_concurrent.db";
    cleanup_db(db_path);

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    /* One handle (one pool), shared by every thread -- this is the
     * scenario a single sqlite3* connection could not serve safely. */
    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    pthread_t threads[BB_POOL_TEST_THREADS];
    bb_pool_worker_arg_t args[BB_POOL_TEST_THREADS];

    for (int i = 0; i < BB_POOL_TEST_THREADS; i++)
    {
        args[i].api = api;
        args[i].h = h;
        args[i].thread_idx = i;
        BB_ASSERT(pthread_create(&threads[i], NULL, bb_pool_worker, &args[i]) == 0);
    }

    int any_failed = 0;
    for (int i = 0; i < BB_POOL_TEST_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
        any_failed |= args[i].failed;
    }
    BB_ASSERT(!any_failed);

    /* Every thread's rows must have landed -- nothing lost or
     * corrupted by connections being borrowed/returned concurrently. */
    void *rows = NULL;
    size_t count = 0;
    BB_ASSERT(api->find_all(h, &user_schema, &rows, &count) == 0);
    BB_ASSERT(count == (size_t)(BB_POOL_TEST_THREADS * BB_POOL_TEST_OPS_PER_THREAD));
    free(rows);

    api->close(h);
}

static void *bb_pool_insert_only_worker(void *arg_)
{
    bb_pool_worker_arg_t *arg = (bb_pool_worker_arg_t *)arg_;
    arg->failed = 0;

    User u = { .id = arg->thread_idx };
    snprintf(u.name, sizeof(u.name), "mem-user-%d", arg->thread_idx);

    if (arg->api->insert(arg->h, &user_schema, &u) != 0)
        arg->failed = 1;

    return NULL;
}

static void test_sqlite_memory_db_shared_across_threads(void)
{
    printf("\tTesting :memory: URI stays a single shared connection under concurrency...\n");

    const bb_model_api_t *api = bb_model_get("sqlite");
    BB_ASSERT(api != NULL);

    /* ":memory:" must NOT be pooled the normal way: a fresh sqlite3
     * connection per ":memory:" open() is its own empty database, so
     * pooling it naively would make each borrowed connection see a
     * different (mostly empty) database. The backend collapses the
     * pool to one shared connection for this URI -- verify rows
     * inserted from different threads are all visible afterward. */
    bb_model_handle_t *h = api->open(":memory:");
    BB_ASSERT(h != NULL);

    pthread_t threads[BB_POOL_TEST_THREADS];
    bb_pool_worker_arg_t args[BB_POOL_TEST_THREADS];

    for (int i = 0; i < BB_POOL_TEST_THREADS; i++)
    {
        args[i].api = api;
        args[i].h = h;
        args[i].thread_idx = i;
        BB_ASSERT(pthread_create(&threads[i], NULL, bb_pool_insert_only_worker, &args[i]) == 0);
    }

    int any_failed = 0;
    for (int i = 0; i < BB_POOL_TEST_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
        any_failed |= args[i].failed;
    }
    BB_ASSERT(!any_failed);

    void *rows = NULL;
    size_t count = 0;
    BB_ASSERT(api->find_all(h, &user_schema, &rows, &count) == 0);
    BB_ASSERT(count == (size_t)BB_POOL_TEST_THREADS);
    free(rows);

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
    test_sqlite_long_table_name_no_overflow();
    test_sqlite_many_fields_no_overflow();
    test_sqlite_concurrent_access_shared_handle();
    test_sqlite_memory_db_shared_across_threads();

    printf("All SQLite model tests passed!\n");
    return 0;
}
