#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

#include "blue-bird/log/persist_logger.h"
#include "blue-bird/persist/key_val.h"

/* ---------------------------
 * Mock Persist Backend
 * --------------------------- */

typedef struct {
    int open_called;
    int close_called;
    int save_called;
    int load_called;

    char stored_key[128];
    char stored_data[65536];
} MockPersistState;

static MockPersistState mock;

/* ---------------------------
 * Mock API
 * --------------------------- */

static bb_persist_kv_handle_t *mock_open(const char *uri)
{
    (void)uri;
    mock.open_called++;
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

static int mock_load(bb_persist_kv_handle_t *h, const char *key, void *out, size_t size)
{
    (void)h;
    (void)key;

    mock.load_called++;

    snprintf(out, size, "%s", mock.stored_data);

    return 0;
}

static bb_persist_kv_api_t mock_api = {
    .name = "mock",
    .open = mock_open,
    .close = mock_close,
    .save = mock_save,
    .load = mock_load,
    .remove = NULL
};

static void reset_mock(void)
{
    memset(&mock, 0, sizeof(mock));
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_persist_logger_writes_log(void)
{
    printf("\tTesting persist logger write...\n");

    reset_mock();

    bb_logger_t logger;

    bb_logger_init_persist(&logger, BB_LOG_LEVEL_TRACE);

    bb_logger_log(&logger, BB_LOG_LEVEL_INFO, "Hello %s", "World");

    BB_ASSERT(mock.open_called >= 2);
    BB_ASSERT(mock.load_called == 1);
    BB_ASSERT(mock.save_called == 1);

    BB_ASSERT(strcmp(mock.stored_key, "bluebird_log") == 0);

    BB_ASSERT(strstr(mock.stored_data, "INFO") != NULL);

    BB_ASSERT(strstr(mock.stored_data, "Hello World") != NULL);

    bb_logger_free_persist_context(&logger);
}

static void test_persist_logger_appends(void)
{
    printf("\tTesting persist logger append...\n");

    reset_mock();

    strcpy(mock.stored_data, "Existing entry\n");

    bb_logger_t logger;

    bb_logger_init_persist(&logger, BB_LOG_LEVEL_TRACE);

    bb_logger_log(&logger, BB_LOG_LEVEL_WARN, "Second entry");

    BB_ASSERT(strstr(mock.stored_data, "Existing entry") != NULL);

    BB_ASSERT(strstr(mock.stored_data, "Second entry") != NULL);

    BB_ASSERT(strstr(mock.stored_data, "WARN") != NULL);

    bb_logger_free_persist_context(&logger);
}

static void test_level_filtering(void)
{
    printf("\tTesting level filtering...\n");

    reset_mock();

    bb_logger_t logger;

    bb_logger_init_persist(&logger, BB_LOG_LEVEL_WARN);

    bb_logger_log(&logger, BB_LOG_LEVEL_DEBUG, "Should not persist");

    BB_ASSERT(mock.save_called == 0);
    BB_ASSERT(mock.load_called == 0);

    bb_logger_free_persist_context(&logger);
}

static void test_free_persist_context(void)
{
    printf("\tTesting persist cleanup...\n");

    bb_logger_t logger;

    bb_logger_init_persist(&logger, BB_LOG_LEVEL_TRACE);

    BB_ASSERT(logger.write != NULL);
    BB_ASSERT(logger.userdata != NULL);

    bb_logger_free_persist_context(&logger);

    BB_ASSERT(logger.write == NULL);
    BB_ASSERT(logger.userdata == NULL);
}

/* ---------------------------
 * Main
 * --------------------------- */

int main(void)
{
    printf("Running persist logger integration tests...\n");

    BB_ASSERT(bb_persist_kv_register(&mock_api) == 0);

    bb_persist_kv_set_default("mock");
    bb_persist_kv_set_default_uri("ignored");

    test_persist_logger_writes_log();
    test_persist_logger_appends();
    test_level_filtering();
    test_free_persist_context();

    printf("All persist logger integration tests passed!\n");
    return 0;
}
