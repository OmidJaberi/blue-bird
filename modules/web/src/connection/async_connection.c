#include "blue-bird/log/log.h"

#include "connection/async_connection.h"
#include "blue-bird/web/error.h"

bb_async_connection_t *bb_async_connection_create(bb_runtime_t *runtime)
{
    if (runtime == NULL)
        return NULL;

    bb_async_connection_t *async_conn = calloc(1, sizeof(*async_conn));
    if (async_conn == NULL)
        return NULL;

    async_conn->runtime = runtime;
    async_conn->disconnected = false;
    return async_conn;
}

void bb_async_connection_destroy(bb_async_connection_t *async_conn)
{
    bb_async_connection_close(async_conn);
    free(async_conn);
}

void bb_async_connection_set_disconnect_callback(bb_async_connection_t *async_conn, bb_async_close_fn callback, void *userdata)
{
    if (!async_conn)
    {
        return;
    }
    async_conn->disconnect = callback;
    async_conn->disconnect_userdata = userdata;
}

static void _bb_async_connection_handle_disconnect(bb_async_connection_t *async_conn)
{
    if (!async_conn || async_conn->disconnected)
    {
        return;
    }

    async_conn->disconnected = true;

    if (async_conn->read_task)
    {
        bb_runtime_cancel_task(async_conn->runtime, async_conn->read_task);
        async_conn->read_task = NULL;
    }

    if (async_conn->write_task)
    {
        bb_runtime_cancel_task(async_conn->runtime, async_conn->write_task);
        async_conn->write_task = NULL;
    }

    if (async_conn->connection)
    {
        async_conn->connection->write_pending = false;
    }

    if (async_conn->disconnect)
    {
        async_conn->disconnect(async_conn->disconnect_userdata);
    }
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

bb_async_connection_t *bb_async_connection_accept(bb_runtime_t *runtime, bb_socket_t server_fd)
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

    _bb_async_connection_handle_disconnect(async_conn);
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
    bb_error_t err = bb_connection_write(conn);
    if (BB_FAILED(err))
    {
        bb_runtime_cancel_task(async_conn->runtime, task);
        async_conn->write_task = NULL;
        conn->write_pending = false;

        if (err.code == BB_ERR_CONNECTION_CLOSED)
        {
            _bb_async_connection_handle_disconnect(async_conn);
        }

        if (async_conn->write_failure)
        {
            async_conn->write_failure(task, async_conn->write_userdata);
        }
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
        return;
    }

    if (!async_conn->connection)
    {
        bb_runtime_cancel_task(async_conn->runtime, task);
        async_conn->read_task = NULL;
        return;
    }

    bb_error_t err = bb_connection_read(async_conn->connection);
    if (BB_FAILED(err))
    {
        switch (err.code)
        {
            case BB_ERR_CONNECTION_CLOSED:
            {
                _bb_async_connection_handle_disconnect(async_conn);
                break;
            }
            case BB_ERR_IO:
            {
                bb_runtime_cancel_task(async_conn->runtime, task);
                async_conn->read_task = NULL;
                if (async_conn->read_error)
                {
                    async_conn->read_error(err, async_conn->read_userdata);
                }
                break;
            }
        }
        return;
    }

    bb_read_status_t status = async_conn->read_step(async_conn->read_userdata);

    if (status.result == BB_READ_ERROR && async_conn->read_error)
    {
        async_conn->read_error(status.err, async_conn->read_userdata);
    }
}

void bb_async_connection_pause_read(bb_async_connection_t *async_conn)
{
    if (!async_conn || !async_conn->read_task)
    {
        return;
    }

    bb_runtime_cancel_task(async_conn->runtime, async_conn->read_task);
    async_conn->read_task = NULL;
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
