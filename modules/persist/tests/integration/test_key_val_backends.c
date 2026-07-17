#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blue-bird/persist/key_val.h"
#include "blue-bird/persist/key_val/persist_file.h"
#include "blue-bird/persist/key_val/persist_json.h"
#include "blue-bird/persist/key_val/persist_sqlite.h"

/* ---------------------------
 * Backend Helpers
 * --------------------------- */

typedef struct {
    const char *name;
    const char *uri;
    int (*register_backend)(void);
    void (*cleanup)(const char *);
} backend_t;

static void cleanup_file(const char *path)
{
    remove(path);
}

static void cleanup_dir(const char *path)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

static backend_t backends[] = {
    {
        .name = "file",
        .uri = "test_file_backend",
        .register_backend = bb_persist_kv_file_register,
        .cleanup = cleanup_dir
    },
    {
        .name = "json",
        .uri = "test_json_backend.json",
        .register_backend = bb_persist_kv_json_register,
        .cleanup = cleanup_file
    },
    {
        .name = "sqlite",
        .uri = "test_sqlite_backend.db",
        .register_backend = bb_persist_kv_sqlite_register,
        .cleanup = cleanup_file
    }
};

#define NUM_BACKENDS (sizeof(backends) / sizeof(backends[0]))

static void setup_backend(const backend_t *backend)
{
    backend->cleanup(backend->uri);

    assert(backend->register_backend() == 0);
    bb_persist_kv_set_default(backend->name);
    bb_persist_kv_set_default_uri(backend->uri);
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_save_and_load(const backend_t *backend)
{
    printf("\tTesting %s save and load...\n", backend->name);


    const char *msg = "hello world";

    assert(bb_persist_kv_save("key", msg, strlen(msg)) == 0);

    char buf[64] = {0};

    assert(bb_persist_kv_load("key", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, msg) == 0);
}

static void test_remove(const backend_t *backend)
{
    printf("\tTesting %s remove...\n", backend->name);


    assert(bb_persist_kv_save("key", "value", 5) == 0);
    assert(bb_persist_kv_remove("key") == 0);

    char buf[64] = {0};

    assert(bb_persist_kv_load("key", buf, sizeof(buf)) != 0);
}

static void test_load_missing_key(const backend_t *backend)
{
    printf("\tTesting %s load missing key...\n", backend->name);


    char buf[64] = {0};

    assert(bb_persist_kv_load("missing", buf, sizeof(buf)) != 0);
}

static void test_overwrite_value(const backend_t *backend)
{
    printf("\tTesting %s overwrite value...\n", backend->name);


    assert(bb_persist_kv_save("key", "first", 5) == 0);
    assert(bb_persist_kv_save("key", "second", 6) == 0);

    char buf[64] = {0};

    assert(bb_persist_kv_load("key", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "second") == 0);
}

static void test_multiple_keys(const backend_t *backend)
{
    printf("\tTesting %s multiple keys...\n", backend->name);


    assert(bb_persist_kv_save("key1", "Alice", 5) == 0);
    assert(bb_persist_kv_save("key2", "Bob", 3) == 0);
    assert(bb_persist_kv_save("key3", "Charlie", 7) == 0);

    char buf[64];

    memset(buf, 0, sizeof(buf));
    assert(bb_persist_kv_load("key1", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "Alice") == 0);

    memset(buf, 0, sizeof(buf));
    assert(bb_persist_kv_load("key2", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "Bob") == 0);

    memset(buf, 0, sizeof(buf));
    assert(bb_persist_kv_load("key3", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "Charlie") == 0);
}

static void test_remove_one_of_multiple_keys(const backend_t *backend)
{
    printf("\tTesting %s remove one of multiple keys...\n", backend->name);

    assert(bb_persist_kv_save("key1", "One", 3) == 0);
    assert(bb_persist_kv_save("key2", "Two", 3) == 0);

    assert(bb_persist_kv_remove("key1") == 0);

    char buf[64] = {0};

    assert(bb_persist_kv_load("key1", buf, sizeof(buf)) != 0);

    memset(buf, 0, sizeof(buf));
    assert(bb_persist_kv_load("key2", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "Two") == 0);
}

static void test_large_buffer(const backend_t *backend)
{
    printf("\tTesting %s large buffer...\n", backend->name);

    char value[512];
    memset(value, 'A', sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';

    assert(bb_persist_kv_save("large", value, strlen(value)) == 0);

    char buf[512] = {0};

    assert(bb_persist_kv_load("large", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, value) == 0);
}

/* ---------------------------
 * Main
 * --------------------------- */

int main(void)
{
    printf("Running key/value backend integration tests...\n");

    for (size_t i = 0; i < NUM_BACKENDS; ++i)
    {
        setup_backend(&backends[i]);

        test_save_and_load(&backends[i]);
        test_remove(&backends[i]);
        test_load_missing_key(&backends[i]);
        test_overwrite_value(&backends[i]);
        test_multiple_keys(&backends[i]);
        test_remove_one_of_multiple_keys(&backends[i]);
        test_large_buffer(&backends[i]);
    }

    printf("All key/value backend integration tests passed!\n");
    return 0;
}
