#include "blue-bird/log/log.h"
#include "websocket/context_internal.h"
#include "websocket/websocket_internal.h"
#include "http/parser.h"

#include "connection.h"
#include "async_connection.h"
#include "server_internal.h"
#include "client_internal.h"

#include <stdio.h>

static bb_error_t default_400(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 400);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Bad Request");
    return BB_SUCCESS();
}

// Write:
typedef void (*bb_async_callback_t)(bb_task_t *, void *);

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

static int _websocket_create_read_task(bb_server_task_data_t *data);
static int _client_create_read_task(_bb_client_task_data_t *data);

static void _client_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    _bb_client_task_data_t *data = userdata;
    _client_create_read_task(data);
}

static void _client_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    _bb_client_task_data_t *data = userdata;
    data->callback(data->client, BB_ERROR(BB_ERR_IO, "Write failed"), data->userdata);
    bb_client_close(data->client);
    free(data);
}

void bb_client_create_write_task(_bb_client_task_data_t *client_data)
{
    _bb_write_task_data_t *write = malloc(sizeof(*write));
    bb_client_t *client = client_data->client;

    if (!client->connection->write_buffer)
    {
        bb_request_serialize(client->req, &client->connection->write_buffer, &client->connection->write_length);
        client->connection->write_offset = 0;
    }

    write->connection = client->connection;
    write->runtime = client->runtime;
    write->success = _client_after_write;
    write->failure = _client_write_error;
    write->userdata = client_data;

    bb_task_t *task = bb_task_create(_bb_write_task, write);
    bb_runtime_watch_fd(client->runtime, client->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
}

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
    _bb_write_task_data_t *write = malloc(sizeof(*write));

    write->connection = data->connection;
    write->runtime = data->runtime;
    write->success = _server_after_write;
    write->failure = _server_write_error;
    write->userdata = data;

    bb_task_t *task = bb_task_create(_bb_write_task, write);
    if (!task)
        return -1;
    bb_runtime_watch_fd(data->runtime, data->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
    return 0;
}

// Read:
typedef enum {
    BB_READ_MORE,
    BB_READ_DONE,
    BB_READ_ERROR,
} bb_read_result_t;

typedef struct {
    bb_read_result_t result;
    bb_error_t err;
} bb_read_status_t;

typedef bb_read_status_t (*bb_read_step_fn)(void *userdata);
typedef void (*bb_read_error_fn)(bb_error_t err, void *userdata);

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

static void _client_read_error(bb_error_t err, void *userdata)
{
    _bb_client_task_data_t *data = userdata;

    data->callback(data->client, err, data->userdata);

    bb_client_close(data->client);
    free(data);
}

static bb_read_status_t _client_read_step(void *userdata)
{
    _bb_client_task_data_t *data = userdata;

    bb_client_t *client = data->client;
    bb_connection_t *conn = client->connection;

    if (!bb_http_message_complete(conn->buffer, conn->buffer_length))
    {
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
    }

    if (bb_response_parse(conn->buffer, client->res))
    {
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Response parse failed") };
    }

    data->callback(client, BB_SUCCESS(), data->userdata);
    bb_client_close(client);
    free(data);

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

static int _client_create_read_task(_bb_client_task_data_t *client)
{
    bb_read_task_data_t *read = malloc(sizeof(*read));

    if (!read)
        return 1;

    read->connection = client->client->connection;
    read->runtime    = client->client->runtime;
    read->userdata   = client;

    read->step  = _client_read_step;
    read->error = _client_read_error;

    bb_task_t *task = bb_task_create(_bb_read_task, read);

    if (!task)
    {
        free(read);
        return 1;
    }

    bb_runtime_watch_fd(read->runtime, read->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, task);
    return 0;
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

    bb_read_task_data_t *read = malloc(sizeof(*read));

    if (!read)
        return 1;

    read->connection = connection;
    read->runtime    = runtime;
    read->userdata   = data;

    read->step  = _server_read_step;
    read->error = _server_read_error;

    bb_task_t *task = bb_task_create(_bb_read_task, read);

    if (!task)
    {
        free(read);
        return 1;
    }

    bb_runtime_watch_fd(read->runtime, read->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, task);
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
