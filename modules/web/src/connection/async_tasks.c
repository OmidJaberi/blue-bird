#include "blue-bird/log/log.h"

#include "connection/async_tasks.h"

// Write:
typedef struct {
    bb_connection_t *connection;
    bb_runtime_t *runtime;

    bb_async_callback_t success;
    bb_async_callback_t failure;

    void *userdata;
} _bb_write_task_data_t;

// Read:
typedef struct {
    bb_connection_t *connection;
    bb_runtime_t *runtime;

    bb_read_step_fn step;
    bb_read_error_fn error;

    void *userdata;
} bb_read_task_data_t;

static void _bb_write_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_write_task_data_t *data = userdata;
    bb_connection_t *conn = data->connection;
    if (bb_connection_write(conn) < 0)
    {
        conn->write_pending = false;
        if (data->failure)
            data->failure(task, data->userdata);

        free(data);
        return;
    }

    if (conn->write_data)
    {
        bb_task_t *next = bb_task_create(_bb_write_task, data);
        bb_runtime_watch_fd(data->runtime, conn->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, next);
        return;
    }

    conn->write_pending = false;
    if (data->success)
        data->success(task, data->userdata);

    free(data);
}

bb_error_t bb_connection_task_create_write(bb_runtime_t *runtime, bb_connection_t *connection, bb_async_callback_t success, bb_async_callback_t failure, void *userdata)
{
    if (!runtime)
        return BB_ERROR(BB_ERR_NULL, "NULL runtime.");
    if (!connection)
        return BB_ERROR(BB_ERR_NULL, "NULL connection.");

    if (connection->write_pending)
        return BB_SUCCESS();

    _bb_write_task_data_t *data = malloc(sizeof(*data));

    if (!data)
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed.");

    data->connection = connection;
    data->runtime = runtime;
    data->success = success;
    data->failure = failure;
    data->userdata = userdata;

    bb_task_t *task = bb_task_create(_bb_write_task, data);
    if (!task)
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate task data.");
    connection->write_pending = true;
    bb_runtime_watch_fd(runtime, connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task); // unwatch?
    return BB_SUCCESS();
}

static void _bb_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_read_task_data_t *data = userdata;

    int n = bb_connection_read(data->connection);
    if (n < 0)
    {
        if (data->error)
            data->error(BB_ERROR(BB_ERR_IO, "Read failed"), data->userdata);
        free(data);
        return;
    }
    if (n == 0 && data->connection->buffer_length == 0)
    {
        // closed
        free(data);
        return;
    }

    bb_read_status_t status = data->step(data->userdata);

    switch (status.result)
    {
        case BB_READ_MORE:
        {
            bb_task_t *next = bb_task_create(_bb_read_task, data);
            bb_runtime_watch_fd(data->runtime, data->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
            return;
        }
        case BB_READ_DONE:
            free(data);
            return;
        case BB_READ_ERROR:
            if (data->error)
                data->error(status.err, data->userdata);
            free(data);
            return;
    }
}

bb_error_t bb_connection_task_create_read(bb_runtime_t *runtime, bb_connection_t *connection, bb_read_step_fn read_step, bb_read_error_fn read_error, void *userdata)
{
    if (!runtime)
        return BB_ERROR(BB_ERR_NULL, "NULL runtime.");
    if (!connection)
        return BB_ERROR(BB_ERR_NULL, "NULL connection.");

    bb_read_task_data_t *read = malloc(sizeof(*read));

    if (!read)
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate task data.");

    read->connection = connection;
    read->runtime    = runtime;
    read->userdata   = userdata;

    read->step  = read_step;
    read->error = read_error;

    bb_task_t *task = bb_task_create(_bb_read_task, read);

    if (!task)
    {
        free(read);
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate task.");
    }

    bb_runtime_watch_fd(read->runtime, read->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, task);
    return BB_SUCCESS();
}
