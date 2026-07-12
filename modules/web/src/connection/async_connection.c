#include "blue-bird/log/log.h"

#include "connection/async_connection.h"

bb_async_connection_t *bb_async_connection_create(bb_runtime_t *runtime)
{
    if (runtime == NULL)
        return NULL;

    bb_async_connection_t *async_conn = calloc(1, sizeof(*async_conn));
    if (async_conn == NULL)
        return NULL;

    async_conn->runtime = runtime;
    return async_conn;
}

void bb_async_connection_destroy(bb_async_connection_t *async_conn)
{
    bb_async_connection_close(async_conn);
    free(async_conn);
}

bb_async_connection_t *bb_async_connection_serve(bb_runtime_t *runtime, int port)
{
    bb_async_connection_t *async_conn = bb_async_connection_create(runtime);
    if (!async_conn)
    {
        return NULL;
    }
    async_conn->connection = bb_connection_serve(port);
    if (!async_conn->connection)
    {
        bb_async_connection_destroy(async_conn);
        return NULL;
    }
    return async_conn;
}

bb_async_connection_t *bb_async_connection_accept(bb_runtime_t *runtime, int server_fd)
{
    bb_async_connection_t *async_conn = bb_async_connection_create(runtime);
    if (!async_conn)
    {
        return NULL;
    }
    async_conn->connection = bb_connection_accept(server_fd);
    if (!async_conn->connection)
    {
        bb_async_connection_destroy(async_conn);
        return NULL;
    }
    return async_conn;
}

bb_async_connection_t *bb_async_connection_connect(bb_runtime_t *runtime, const char *host, const char *port_str)
{
    bb_async_connection_t *async_conn = bb_async_connection_create(runtime);
    if (!async_conn)
    {
        return NULL;
    }
    async_conn->connection = bb_connection_connect_nonblocking(host, port_str);
    if (!async_conn->connection)
    {
        bb_async_connection_destroy(async_conn);
        return NULL;
    }
    return async_conn;
}

void bb_async_connection_close(bb_async_connection_t *async_conn)
{
    if (!async_conn)
    {
        return;
    }
    bb_connection_destroy(async_conn->connection);
    async_conn->connection = NULL;

    bb_runtime_cancel_task(async_conn->runtime, async_conn->read_task);
    async_conn->read_task = NULL;

    bb_runtime_cancel_task(async_conn->runtime, async_conn->write_task);
    async_conn->write_task = NULL;
}

static void _bb_write_task(bb_task_t *task, void *userdata)
{
    bb_async_connection_t *async_conn = userdata;
    if (!async_conn)
    {
        return;
    }
    if (!async_conn->connection)
    {
        bb_runtime_cancel_task(async_conn->runtime, task);
        async_conn->write_task = NULL;
        return;
    }
    bb_connection_t *conn = async_conn->connection;
    if (bb_connection_write(conn) < 0)
    {
        bb_runtime_cancel_task(async_conn->runtime, task);
        async_conn->write_task = NULL;
        conn->write_pending = false;
        if (async_conn->write_failure)
            async_conn->write_failure(task, async_conn->write_userdata);
        return;
    }

    if (conn->write_data)
    {
        return;
    }

    bb_runtime_cancel_task(async_conn->runtime, task);
    async_conn->write_task = NULL;
    conn->write_pending = false;
    if (async_conn->write_success)
    {
        async_conn->write_success(task, async_conn->write_userdata);
    }
}

bb_error_t bb_async_connection_create_write_task(bb_async_connection_t *async_conn, bb_async_callback_t success, bb_async_callback_t failure, void *userdata)
{
    if (!async_conn || !async_conn->connection)
        return BB_ERROR(BB_ERR_NULL, "No connection.");

    if (async_conn->connection->write_pending)
        return BB_SUCCESS();

    async_conn->write_success = success;
    async_conn->write_failure = failure;
    async_conn->write_userdata = userdata;

    bb_task_t *task = bb_runtime_watch_fd(async_conn->runtime, async_conn->connection->fd, BB_EVENT_WRITE, BB_WATCH_PERSISTENT, _bb_write_task, async_conn);
    if (!task)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to create task.");
    }

    // Handle existing task...
    async_conn->write_task = task;
    async_conn->connection->write_pending = true;

    return BB_SUCCESS();
}

static void _bb_read_task(bb_task_t *task, void *userdata)
{
    bb_async_connection_t *async_conn = userdata;

    if (!async_conn)
    {
        // bb_runtime_cancel_task(async_conn->runtime, task);
        return;
    }

    if (!async_conn->connection)
    {
        bb_runtime_cancel_task(async_conn->runtime, task);
        async_conn->read_task = NULL;
        return;
    }

    int n = bb_connection_read(async_conn->connection);
    if (n < 0)
    {
        bb_runtime_cancel_task(async_conn->runtime, task);
        async_conn->read_task = NULL;
        if (async_conn->read_error)
        {
            async_conn->read_error(BB_ERROR(BB_ERR_IO, "Read failed"), async_conn->read_userdata);
        }
        return;
    }
    if (n == 0 && async_conn->connection->buffer_length == 0)
    {
        // closed
        bb_runtime_cancel_task(async_conn->runtime, task);
        async_conn->read_task = NULL;
        return;
    }

    bb_read_status_t status = async_conn->read_step(async_conn->read_userdata);

    if (status.result == BB_READ_ERROR && async_conn->read_error)
    {
        async_conn->read_error(status.err, async_conn->read_userdata);
    }
}

bb_error_t bb_async_connection_create_read_task(bb_async_connection_t *async_conn, bb_read_step_fn read_step, bb_read_error_fn read_error, void *userdata)
{
    if (!async_conn || !async_conn->connection)
    {
        return BB_ERROR(BB_ERR_NULL, "No connection.");
    }

    async_conn->read_step = read_step;
    async_conn->read_error = read_error;
    async_conn->read_userdata = userdata;

    if (async_conn->read_task != NULL)
    {
        return BB_SUCCESS();
    }

    bb_task_t *task = bb_runtime_watch_fd(async_conn->runtime, async_conn->connection->fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, _bb_read_task, async_conn);
    if (!task)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate task.");
    }

    async_conn->read_task = task;

    return BB_SUCCESS();
}
