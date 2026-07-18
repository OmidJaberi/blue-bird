#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "blue-bird/log/log.h"

/* ---------------------------
 * Mock State
 * --------------------------- */

typedef struct {
    int write_called;
    bb_log_level_t last_level;
    char last_message[256];
} MockState;

static MockState mock;

/* ---------------------------
 * Mock Backend
 * --------------------------- */

static void mock_write(bb_logger_t *logger, bb_log_level_t level, const char *fmt, va_list args)
{
    (void) logger;

    mock.write_called++;
    mock.last_level = level;

    vsnprintf(mock.last_message, sizeof(mock.last_message), fmt, args);
}

static void reset_mock(void)
{
    memset(&mock, 0, sizeof(mock));
}

/* ---------------------------
 * Tests
 * --------------------------- */

static void test_log_calls_backend(void)
{
    printf("\tTesting log calls backend...\n");

    reset_mock();

    bb_logger_t logger = {
        .level = BB_LOG_LEVEL_INFO,
        .write = mock_write,
        .userdata = NULL
    };

    bb_logger_log(&logger, BB_LOG_LEVEL_INFO, "Hello %s", "World");

    BB_ASSERT(mock.write_called == 1);
    BB_ASSERT(mock.last_level == BB_LOG_LEVEL_INFO);
    BB_ASSERT(strcmp(mock.last_message, "Hello World") == 0);
}

static void test_log_filters_level(void)
{
    printf("\tTesting level filtering...\n");

    reset_mock();

    bb_logger_t logger = {
        .level = BB_LOG_LEVEL_WARN,
        .write = mock_write,
        .userdata = NULL
    };

    bb_logger_log(&logger, BB_LOG_LEVEL_DEBUG, "Should not appear");

    BB_ASSERT(mock.write_called == 0);
}

static void test_log_null_logger(void)
{
    printf("\tTesting NULL logger...\n");

    reset_mock();

    bb_logger_log(NULL, BB_LOG_LEVEL_INFO, "Ignored");

    BB_ASSERT(mock.write_called == 0);
}

static void test_log_null_callback(void)
{
    printf("\tTesting NULL callback...\n");

    reset_mock();

    bb_logger_t logger = {
        .level = BB_LOG_LEVEL_TRACE,
        .write = NULL,
        .userdata = NULL
    };

    bb_logger_log(&logger, BB_LOG_LEVEL_INFO, "Ignored");

    BB_ASSERT(mock.write_called == 0);
}

/* Helper for testing bb_logger_vlog */
static void call_vlog(bb_logger_t *logger, bb_log_level_t level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    bb_logger_vlog(logger, level, fmt, args);
    va_end(args);
}

static void test_vlog_calls_backend(void)
{
    printf("\tTesting vlog...\n");

    reset_mock();

    bb_logger_t logger = {
        .level = BB_LOG_LEVEL_TRACE,
        .write = mock_write,
        .userdata = NULL
    };

    call_vlog(&logger, BB_LOG_LEVEL_INFO, "Value=%d", 42);

    BB_ASSERT(mock.write_called == 1);
    BB_ASSERT(strcmp(mock.last_message, "Value=42") == 0);
}

static void test_close_logger(void)
{
    printf("\tTesting close logger...\n");

    bb_logger_t logger = {
        .level = BB_LOG_LEVEL_TRACE,
        .write = mock_write,
        .userdata = (void *)0x1234
    };

    bb_logger_close(&logger);

    BB_ASSERT(logger.write == NULL);
    BB_ASSERT(logger.userdata == NULL);
}

static void test_log_after_close(void)
{
    printf("\tTesting log after close...\n");

    reset_mock();

    bb_logger_t logger = {
        .level = BB_LOG_LEVEL_TRACE,
        .write = mock_write,
        .userdata = NULL
    };

    bb_logger_close(&logger);

    bb_logger_log(&logger, BB_LOG_LEVEL_INFO, "Ignored");

    BB_ASSERT(mock.write_called == 0);
}

static void test_default_logger_macro(void)
{
    printf("\tTesting default logger macro...\n");

    reset_mock();

    default_logger.level = BB_LOG_LEVEL_TRACE;
    default_logger.write = mock_write;
    default_logger.userdata = NULL;

    BB_LOG_INFO("macro test");

    BB_ASSERT(mock.write_called == 1);
    BB_ASSERT(strcmp(mock.last_message, "macro test") == 0);
}

/* ---------------------------
 * Main
 * --------------------------- */

int main(void)
{
    printf("Running logger tests...\n");

    test_log_calls_backend();
    test_log_filters_level();
    test_log_null_logger();
    test_log_null_callback();
    test_vlog_calls_backend();
    test_close_logger();
    test_log_after_close();
    test_default_logger_macro();

    printf("All logger tests passed!\n");

    return 0;
}
