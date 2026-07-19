#include "connection/async_connection.h"
#include "blue-bird/runtime/runtime.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>
#include <stdio.h>

static bb_read_status_t dummy_read_step(void *userdata)
{
    (void)userdata;

    bb_read_status_t status;
    status.result = BB_READ_DONE;
    return status;
}

static void dummy_error(bb_error_t err, void *userdata)
{
    (void)err;
    (void)userdata;
}

static void dummy_callback(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

static void async_connection_create_test(void)
{
    printf("\tTesting async connection creation...\n");

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *conn =
        bb_async_connection_create(runtime);

    BB_ASSERT(conn != NULL);
    BB_ASSERT(conn->runtime == runtime);
    BB_ASSERT(conn->connection == NULL);
    BB_ASSERT(conn->read_task == NULL);
    BB_ASSERT(conn->write_task == NULL);

    bb_async_connection_destroy(conn);
    bb_runtime_destroy(runtime);
}

static void async_connection_null_runtime_test(void)
{
    printf("\tTesting NULL runtime...\n");

    BB_ASSERT(bb_async_connection_create(NULL) == NULL);
}

static void async_connection_close_empty_test(void)
{
    printf("\tTesting close without connection...\n");

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *conn =
        bb_async_connection_create(runtime);

    bb_async_connection_close(conn);

    bb_async_connection_destroy(conn);
    bb_runtime_destroy(runtime);
}

static void async_connection_write_task_invalid_test(void)
{
    printf("\tTesting invalid write task creation...\n");

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *conn =
        bb_async_connection_create(runtime);

    BB_ASSERT(BB_FAILED(
        bb_async_connection_create_write_task(
            conn,
            dummy_callback,
            dummy_callback,
            NULL)));

    bb_async_connection_destroy(conn);
    bb_runtime_destroy(runtime);
}

static void async_connection_read_task_invalid_test(void)
{
    printf("\tTesting invalid read task creation...\n");

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *conn =
        bb_async_connection_create(runtime);

    BB_ASSERT(BB_FAILED(
        bb_async_connection_create_read_task(
            conn,
            dummy_read_step,
            dummy_error,
            NULL)));

    bb_async_connection_destroy(conn);
    bb_runtime_destroy(runtime);
}

static void async_connection_close_test(void)
{
    printf("\tTesting async connection close...\n");

    int fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_runtime_t *runtime = bb_runtime_create();

    bb_async_connection_t *conn =
        bb_async_connection_create(runtime);

    conn->connection = bb_connection_create(fds[0]);

    bb_async_connection_close(conn);

    BB_ASSERT(conn->connection == NULL);
    BB_ASSERT(conn->read_task == NULL);
    BB_ASSERT(conn->write_task == NULL);

    bb_socket_close(fds[1]);

    bb_async_connection_destroy(conn);
    bb_runtime_destroy(runtime);
}

int main(void)
{
    printf("Running async connection unit tests...\n");

    async_connection_create_test();
    async_connection_null_runtime_test();
    async_connection_close_empty_test();
    async_connection_write_task_invalid_test();
    async_connection_read_task_invalid_test();
    async_connection_close_test();

    printf("All async connection tests passed.\n");

    return 0;
}
