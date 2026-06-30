#include "blue-bird/log/log.h"
#include "websocket/context_internal.h"
#include "websocket/websocket_internal.h"
#include "http/parser.h"

#include "connection.h"
#include "async_connection.h"
#include "server_internal.h"
#include "client_internal.h"

#include <stdio.h>

typedef struct {
    bb_server_t *server;
    bb_connection_t *connection;
    bb_runtime_t *runtime;
    bb_ws_session_t *ws_session;
} bb_server_task_data_t;

static bb_error_t default_400(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 400);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Bad Request");
    return BB_SUCCESS();
}

// Write:
typedef struct {
    bb_connection_t *connection;
    bb_runtime_t *runtime;

    bb_async_callback_t success;
    bb_async_callback_t failure;

    void *userdata;
} _bb_write_task_data_t;

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

static int _websocket_create_read_task(bb_server_task_data_t *data);

static void _server_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_task_data_t *data = userdata;
    if (data->ws_session)
    {
        /*
        * Now websocket session owns the connection.
        */
        data->connection = NULL;
        int rc = _websocket_create_read_task(data);
        if (rc != 0)
        {
            bb_ws_session_destroy(data->ws_session);
            free(data);
        }
        return;
    }
    bb_connection_destroy(data->connection);

    free(data);
}

static void _server_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_task_data_t *data = userdata;
    bb_connection_destroy(data->connection);
    free(data);
}

static int _server_create_write_task(bb_server_task_data_t *data)
{
    if (BB_FAILED(bb_connection_task_create_write(data->runtime, data->connection, _server_after_write, _server_write_error, data)))
        return -1;
    return 0;
}

// Read:
typedef struct {
    bb_connection_t *connection;
    bb_runtime_t *runtime;

    bb_read_step_fn step;
    bb_read_error_fn error;

    void *userdata;
} bb_read_task_data_t;

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

static void _server_read_error(bb_error_t err, void *userdata)
{
    (void)err;

    bb_server_task_data_t *data = userdata;

    BB_LOG_ERROR("%s: %s\n", bb_strerror(err.code), err.msg);

    bb_connection_destroy(data->connection);
    free(data);
}

static bb_read_status_t _server_read_step(void *userdata)
{
    bb_server_task_data_t *data = userdata;
    bb_connection_t *connection = data->connection;

    if (!bb_http_message_complete(connection->buffer, connection->buffer_length))
    {
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
    }

    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    if (bb_request_parse(connection->buffer, req))
    {
        default_400(req, res);
    }
    else
    {
        bb_error_t err = bb_server_run_request_pipeline(data->server, connection, &data->ws_session, req, res);

        if (BB_FAILED(err))
        {
            bb_request_destroy(req);
            bb_response_destroy(res);

            return (bb_read_status_t){ BB_READ_ERROR, err };
        }
    }

    bb_response_serialize(res, &connection->write_buffer, &connection->write_length);

    bb_request_destroy(req);
    bb_response_destroy(res);

    connection->write_offset = 0;
    connection->state = BB_CONNECTION_WRITING;

    if (_server_create_write_task(data))
    {
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") };
    }

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

static int _server_create_read_task(bb_server_t *server, bb_connection_t *connection, bb_runtime_t *runtime)
{
    bb_server_task_data_t *data = malloc(sizeof(*data));
    if (!data) return 1;

    data->server = server;
    data->connection = connection;
    data->runtime = runtime;
    data->ws_session = NULL;

    if (BB_FAILED(bb_connection_task_create_read(runtime, connection, _server_read_step, _server_read_error, data)))
        return 1;
    return 0;
}

static void _websocket_read_error(bb_error_t err, void *userdata)
{
    (void)err;
    bb_server_task_data_t *data = userdata;
    bb_ws_session_destroy(data->ws_session);
    free(data);
}

static bb_read_status_t _websocket_read_step(void *userdata)
{
    bb_server_task_data_t *data = userdata;
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
        if (!data->connection)
            data->connection = session->connection;

        if (_server_create_write_task(data))
        {
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") };
        }
    }
    else
    {
        if (_websocket_create_read_task(data) != 0)
        {
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule read task.") };
        }
    }

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

static int _websocket_create_read_task(bb_server_task_data_t *data)
{
    bb_read_task_data_t *read = malloc(sizeof(*read));

    if (!read)
    {
        return 1;
    }

    read->connection = data->ws_session->connection;
    read->runtime    = data->runtime;
    read->userdata   = data;

    read->step  = _websocket_read_step;
    read->error = _websocket_read_error;

    bb_task_t *task = bb_task_create(_bb_read_task, read);
    if (!task)
    {
        return 1;
    }

    bb_runtime_watch_fd(read->runtime, read->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, task);
    return 0;
}

// Accept:
void bb_accept_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;

    bb_connection_t *connection;
    while ((connection = bb_connection_accept(data->connection->fd)))
    {
        if (_server_create_read_task(data->server, connection, data->runtime) != 0)
        {
            bb_connection_destroy(connection);
        }
    }
}

bb_error_t bb_server_create_accept_task(bb_server_t *server)
{
    bb_server_task_data_t *data = malloc(sizeof(*data));

    if (!data)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to create task.");
    }

    data->server = server;
    data->runtime = server->runtime;
    data->connection = server->connection;

    bb_task_t *task = bb_task_create(bb_accept_task, data);

    bb_runtime_watch_fd(server->runtime, server->connection->fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, task);
    return BB_SUCCESS();
}
