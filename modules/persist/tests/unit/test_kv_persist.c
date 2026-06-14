#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "blue-bird/persist/key_val.h"

/* ---------------------------
 * Mock State
 * --------------------------- */

typedef struct {
    int open_called;
    int close_called;
    int save_called;
    int load_called;
    int remove_called;

    char last_uri[256];
    char stored_key[128];
    char stored_data[1024];
} MockState;

static MockState mock;

/* ---------------------------
 * Mock Backend
 * --------------------------- */

static void reset_mock(void)
{
    memset(&mock, 0, sizeof(mock));
}

static bb_persist_kv_handle_t *mock_open(const char *uri)
{
    mock.open_called++;

    if (uri)
    {
        snprintf(mock.last_uri, sizeof(mock.last_uri), "%s", uri);
    }

    return (bb_persist_kv_handle_t *)&mock;
}

static void mock_close(bb_persist_kv_handle_t *h)
{
    (void)h;
    mock.close_called++;
}

static int mock_save(bb_persist_kv_handle_t *h, const char *key, const void *data, size_t size)
{
    (void)h;

    mock.save_called++;

    snprintf(mock.stored_key, sizeof(mock.stored_key), "%s", key);

    size_t n = size;
    if (n >= sizeof(mock.stored_data))
        n = sizeof(mock.stored_data) - 1;

    memcpy(mock.stored_data, data, n);
    mock.stored_data[n] = '\0';

    return 0;
}

static int mock_load(bb_persist_kv_handle_t *h, const char *key, void *buf, size_t bufsize)
{
    (void)h;

    mock.load_called++;

    assert(strcmp(key, mock.stored_key) == 0);

    snprintf(buf, bufsize, "%s", mock.stored_data);

    return 0;
}

static int mock_remove(bb_persist_kv_handle_t *h, const char *key)
{
    (void)h;

    mock.remove_called++;

    if (strcmp(key, mock.stored_key) == 0)
    {
        mock.stored_key[0] = '\0';
        mock.stored_data[0] = '\0';
    }

    return 0;
}

static bb_persist_kv_api_t mock_api = {
    .name = "mock",
    .open = mock_open,
    .close = mock_close,
    .save = mock_save,
    .load = mock_load,
    .remove = mock_remove
};

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_backend_registration(void)
{
    printf("\tTesting backend registration...\n");

    const bb_persist_kv_api_t *api = persist_get("mock");

    assert(api != NULL);
    assert(strcmp(api->name, "mock") == 0);
}

static void test_default_backend_configuration(void)
{
    printf("\tTesting default backend configuration...\n");

    bb_persist_kv_set_default("mock");

    assert(strcmp(bb_persist_kv_get_default(), "mock") == 0);
}

static void test_save_wrapper(void)
{
    printf("\tTesting save wrapper...\n");

    reset_mock();

    const char value[] = "hello world";

    assert(bb_persist_kv_save("greeting", value, strlen(value)) == 0);

    assert(mock.open_called == 1);
    assert(mock.save_called == 1);
    assert(mock.close_called == 1);

    assert(strcmp(mock.stored_key, "greeting") == 0);

    assert(strcmp(mock.stored_data, value) == 0);

    assert(strcmp(mock.last_uri, "test.db") == 0);
}

static void test_load_wrapper(void)
{
    printf("\tTesting load wrapper...\n");

    reset_mock();

    strcpy(mock.stored_key, "answer");
    strcpy(mock.stored_data, "42");

    char buf[32] = {0};

    assert(bb_persist_kv_load("answer", buf, sizeof(buf)) == 0);

    assert(mock.open_called == 1);
    assert(mock.load_called == 1);
    assert(mock.close_called == 1);

    assert(strcmp(buf, "42") == 0);
}

static void test_remove_wrapper(void)
{
    printf("\tTesting remove wrapper...\n");

    reset_mock();

    strcpy(mock.stored_key, "temp");
    strcpy(mock.stored_data, "value");

    assert(bb_persist_kv_remove("temp") == 0);

    assert(mock.open_called == 1);
    assert(mock.remove_called == 1);
    assert(mock.close_called == 1);

    assert(mock.stored_key[0] == '\0');
    assert(mock.stored_data[0] == '\0');
}

static void test_open_close_per_operation(void)
{
    printf("\tTesting open/close lifecycle...\n");

    reset_mock();

    bb_persist_kv_save("k", "v", 1);

    assert(mock.open_called == 1);
    assert(mock.close_called == 1);

    bb_persist_kv_load("k", mock.stored_data, sizeof(mock.stored_data));

    assert(mock.open_called == 2);
    assert(mock.close_called == 2);

    bb_persist_kv_remove("k");

    assert(mock.open_called == 3);
    assert(mock.close_called == 3);
}

/* ---------------------------
 * Main
 * --------------------------- */

int main(void)
{
    printf("Running key/value persistence tests...\n");

    assert(bb_persist_kv_register(&mock_api) == 0);

    bb_persist_kv_set_default("mock");
    bb_persist_kv_set_default_uri("test.db");

    test_backend_registration();
    test_default_backend_configuration();
    test_save_wrapper();
    test_load_wrapper();
    test_remove_wrapper();
    test_open_close_per_operation();

    printf("All key/value persistence tests passed!\n");

    return 0;
}
