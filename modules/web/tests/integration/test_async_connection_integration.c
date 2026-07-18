#include "connection/async_connection.h"
#include "blue-bird/runtime/runtime.h"

#include <blue-bird/error/assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_PORT 18081

static volatile int finished = 0;
static bb_async_connection_t *listener = NULL;

/* ============================================================
 * Server
 * ============================================================ */

static void *server_thread(void *arg)
{
    (void)arg;

    bb_runtime_t *runtime = bb_runtime_create();

    listener = bb_async_connection_serve(runtime, TEST_PORT);
    BB_ASSERT(listener);

    while (!finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_async_connection_destroy(listener);
    bb_runtime_destroy(runtime);

    return NULL;
}

/* ============================================================
 * Helpers
 * ============================================================ */

static volatile int read_called = 0;
static volatile int write_called = 0;
static volatile int error_called = 0;

static bb_read_status_t read_done(void *userdata)
{
    bb_async_connection_t *conn = userdata;

    BB_ASSERT(conn->connection->buffer_length > 0);

    read_called = 1;

    bb_read_status_t status = {
        .result = BB_READ_DONE
    };

    bb_runtime_cancel_task(conn->runtime, conn->read_task);
    conn->read_task = NULL;
    return status;
}

static bb_read_status_t read_more(void *userdata)
{
    bb_async_connection_t *conn = userdata;

    if (conn->connection->buffer_length < 5)
    {
        return (bb_read_status_t){ .result = BB_READ_MORE };
    }

    read_called = 1;

    bb_runtime_cancel_task(conn->runtime, conn->read_task);
    conn->read_task = NULL;
    return (bb_read_status_t){ .result = BB_READ_DONE };
}

static void read_error(bb_error_t err, void *userdata)
{
    (void)err;
    (void)userdata;

    error_called = 1;
}

static void write_success(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    write_called = 1;
}

static void write_failure(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    BB_ASSERT(0 && "unexpected write failure");
}

/* ============================================================
 * Tests
 * ============================================================ */

static void async_connect_accept_test(void)
{
    printf("\tTesting async connect/accept...\n");

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *client = bb_async_connection_connect(runtime, "127.0.0.1", "18081");

    BB_ASSERT(client);

    bb_async_connection_t *server = NULL;

    while (!server)
    {
        server = bb_async_connection_accept(
            runtime,
            listener->connection->fd);

        bb_runtime_tick(runtime);
        usleep(1000);
    }

    BB_ASSERT(server);

    bb_async_connection_destroy(client);
    bb_async_connection_destroy(server);
    bb_runtime_destroy(runtime);
}

static void async_write_callback_test(void)
{
    printf("\tTesting async write callback...\n");

    write_called = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *client = bb_async_connection_connect(runtime, "127.0.0.1", "18081");

    BB_ASSERT(client);

    bb_async_connection_t *server = NULL;

    while (!server)
    {
        server = bb_async_connection_accept(runtime, listener->connection->fd);

        bb_runtime_tick(runtime);
    }

    char *msg = strdup("hello");

    bb_connection_buffer_add(client->connection, msg, 6);

    bb_error_t err = bb_async_connection_create_write_task(client, write_success, write_failure, NULL);

    BB_ASSERT(!BB_FAILED(err));

    while (!write_called)
    {
        bb_runtime_tick(runtime);
        usleep(1000);
    }

    bb_async_connection_destroy(client);
    bb_async_connection_destroy(server);
    bb_runtime_destroy(runtime);
}

static void async_read_callback_test(void)
{
    printf("\tTesting async read callback...\n");

    read_called = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *client = bb_async_connection_connect(runtime, "127.0.0.1", "18081");

    BB_ASSERT(client);

    bb_async_connection_t *server = NULL;

    while (!server)
    {
        server = bb_async_connection_accept(runtime, listener->connection->fd);

        bb_runtime_tick(runtime);
    }

    bb_error_t err = bb_async_connection_create_read_task(server, read_done, read_error, server);

    BB_ASSERT(!BB_FAILED(err));

    char *msg = strdup("hello");

    bb_connection_buffer_add(client->connection, msg, 6);
    bb_connection_write(client->connection);

    while (!read_called)
    {
        bb_runtime_tick(runtime);
        usleep(1000);
    }

    BB_ASSERT(error_called == 0);

    bb_async_connection_destroy(client);
    bb_async_connection_destroy(server);
    bb_runtime_destroy(runtime);
}

static void async_read_more_test(void)
{
    printf("\tTesting BB_READ_MORE flow...\n");

    read_called = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *client = bb_async_connection_connect(runtime, "127.0.0.1", "18081");

    BB_ASSERT(client);

    bb_async_connection_t *server = NULL;

    while (!server)
    {
        server = bb_async_connection_accept(runtime, listener->connection->fd);

        bb_runtime_tick(runtime);
    }

    bb_async_connection_create_read_task(server, read_more, read_error, server);

    char *msg = strdup("hello");

    bb_connection_buffer_add(client->connection, msg, 6);
    bb_connection_write(client->connection);

    while (!read_called)
    {
        bb_runtime_tick(runtime);
        usleep(1000);
    }

    BB_ASSERT(server->read_task == NULL);

    bb_async_connection_destroy(client);
    bb_async_connection_destroy(server);
    bb_runtime_destroy(runtime);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    pthread_t thread;

    BB_ASSERT(pthread_create(&thread, NULL, server_thread, NULL) == 0);

    while (!listener)
    {
        usleep(1000);
    }

    printf("Running async connection integration tests...\n");

    async_connect_accept_test();
    async_write_callback_test();
    async_read_callback_test();
    async_read_more_test();

    finished = 1;

    pthread_join(thread, NULL);

    printf("All async connection integration tests passed.\n");

    return 0;
}
