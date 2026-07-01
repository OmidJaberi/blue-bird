#include "blue-bird/log/log.h"
#include "websocket/context_internal.h"
#include "websocket/websocket_internal.h"

#include "connection.h"
#include "async_connection.h"
#include "websocket/session.h"

typedef struct {
    bb_runtime_t *runtime;
    bb_ws_session_t *ws_session;
} bb_ws_task_data_t;

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
        if (data->failure)
            data->failure(task, data->userdata);

        free(data);
        return;
    }

    if (conn->write_offset < conn->write_length)
    {
        bb_task_t *next = bb_task_create(_bb_write_task, data);
        bb_runtime_watch_fd(data->runtime, conn->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, next);
        return;
    }

    if (data->success)
        data->success(task, data->userdata);

    free(data);
}

bb_error_t bb_connection_task_create_write(bb_runtime_t *runtime, bb_connection_t *conn, bb_async_callback_t success, bb_async_callback_t failure, void *userdata)
{
    _bb_write_task_data_t *data = malloc(sizeof(*data));

    data->connection = conn;
    data->runtime = runtime;
    data->success = success;
    data->failure = failure;
    data->userdata = userdata;

    bb_task_t *task = bb_task_create(_bb_write_task, data);
    if (!task)
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate task data.");
    bb_runtime_watch_fd(runtime, conn->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
    return BB_SUCCESS();
}

static void _bb_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_read_task_data_t *data = userdata;

    if (bb_connection_read(data->connection) < 0)
    {
        if (data->error)
            data->error(BB_ERROR(BB_ERR_IO, "Read failed"), data->userdata);
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

static void _websocket_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_ws_task_data_t *data = userdata;
    bb_error_t err = bb_connection_task_create_ws_read(data->runtime, data->ws_session->connection, data->ws_session->handler);
    if (BB_FAILED(err))
    {
        bb_ws_session_destroy(data->ws_session);
    }
    free(data);
}

static void _websocket_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_ws_task_data_t *data = userdata;
    bb_connection_destroy(data->ws_session->connection);
    free(data);
}

static void _websocket_read_error(bb_error_t err, void *userdata)
{
    (void)err;
    bb_ws_task_data_t *data = userdata;
    bb_ws_session_destroy(data->ws_session);
    free(data);
}

static bb_read_status_t _websocket_read_step(void *userdata)
{
    bb_ws_task_data_t *data = userdata;
    bb_ws_session_t *session = data->ws_session;

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frame(session->websocket, &frame);

    if (err.code == BB_ERR_INTERNAL)
    {
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
    }

    if (BB_FAILED(err))
    {
        return (bb_read_status_t){ BB_READ_ERROR, err };
    }

    bb_ws_message_t msg;

    err = bb_ws_frame_to_message(&frame, &msg);

    if (BB_FAILED(err))
    {
        bb_ws_frame_destroy(&frame);
        return (bb_read_status_t){ BB_READ_ERROR, err };
    }

    session->handler(&session->context, &msg);

    bb_ws_frame_destroy(&frame);

    if (session->connection->write_buffer)
    {
        if (BB_FAILED(bb_connection_task_create_write(data->runtime, session->connection, _websocket_after_write, _websocket_write_error, data)))
        {
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") };
        }
    }
    else
    {
        if (BB_FAILED(bb_connection_task_create_ws_read(data->runtime, session->connection, session->handler)))
        {
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule read task.") };
        }
    }

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

bb_error_t bb_connection_task_create_ws_read(bb_runtime_t *runtime, bb_connection_t *connection, bb_ws_handler_cb handler)
{
    bb_ws_task_data_t *data = malloc(sizeof(*data));
    if (!data)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate.");
    }
    data->runtime = runtime;
    data->ws_session = bb_ws_session_create(connection, handler);
    if (!data->ws_session)
    {
        free(data);
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate.");
    }

    bb_error_t err = bb_connection_task_create_read(runtime, connection, _websocket_read_step, _websocket_read_error, data);
    if (BB_FAILED(err))
    {
        bb_ws_session_destroy(data->ws_session);
        free(data);
    }
    return err;
}
