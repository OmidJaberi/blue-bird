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
} bb_write_task_data_t;

static void bb_write_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_write_task_data_t *data = userdata;
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
        bb_task_t *next = bb_task_create(bb_write_task, data);
        bb_runtime_watch_fd(data->runtime, conn->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, next);
        return;
    }

    if (data->success)
        data->success(task, data->userdata);

    free(data);
}

static void _bb_websocket_read_task(bb_task_t *task, void *userdata);
static void _bb_client_create_read_task(_bb_client_task_data_t *data);

static void client_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    _bb_client_task_data_t *data = userdata;
    _bb_client_create_read_task(data);
}

static void client_error(bb_task_t *task, void *userdata)
{
    (void) task;
    _bb_client_task_data_t *data = userdata;
    data->callback(data->client, BB_ERROR(BB_ERR_IO, "Write failed"), data->userdata);
    bb_client_close(data->client);
    free(data);
}

void bb_client_create_write_task(_bb_client_task_data_t *client_data)
{
    bb_write_task_data_t *write = malloc(sizeof(*write));
    bb_client_t *client = client_data->client;

    if (!client->connection->write_buffer)
    {
        bb_request_serialize(client->req, &client->connection->write_buffer, &client->connection->write_length);
        client->connection->write_offset = 0;
    }

    write->connection = client->connection;
    write->runtime = client->runtime;
    write->success = client_after_write;
    write->failure = client_error;
    write->userdata = client_data;

    bb_task_t *task = bb_task_create(bb_write_task, write);
    bb_runtime_watch_fd(client->runtime, client->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
}

static void server_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_task_data_t *data = userdata;
    if (data->ws_session)
    {
        /*
        * Now websocket session owns the connection.
        */
        data->connection = NULL;
        bb_task_t *task = bb_task_create(_bb_websocket_read_task, data);
        if (!task)
        {
            bb_ws_session_destroy(data->ws_session);
            free(data);
            return;
        }
        bb_runtime_watch_fd(data->runtime, data->ws_session->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, task);
        return;
    }
    bb_connection_destroy(data->connection);

    free(data);
}

static void server_error(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_task_data_t *data = userdata;
    bb_connection_destroy(data->connection);
    free(data);
}

static int _bb_server_create_write_task(bb_server_task_data_t *data)
{
    bb_write_task_data_t *write = malloc(sizeof(*write));

    write->connection = data->connection;
    write->runtime = data->runtime;
    write->success = server_after_write;
    write->failure = server_error;
    write->userdata = data;

    bb_task_t *task = bb_task_create(bb_write_task, write);
    if (!task)
        return -1;
    bb_runtime_watch_fd(data->runtime, data->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
    return 0;
}

// Read:
typedef bool (*bb_read_complete_fn)(void *userdata);

typedef bb_error_t (*bb_read_process_fn)(void *userdata);

typedef void (*bb_read_error_fn)(bb_error_t err, void *userdata);

typedef struct {
    bb_connection_t *connection;
    bb_runtime_t *runtime;

    bb_read_complete_fn complete;
    bb_read_process_fn process;
    bb_read_error_fn error;

    void *userdata;
} bb_read_task_data_t;

static void bb_read_task(bb_task_t *task, void *userdata)
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

    if (!data->complete(data->userdata))
    {
        bb_task_t *next = bb_task_create(bb_read_task, data);
        bb_runtime_watch_fd(data->runtime, data->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
        return;
    }

    bb_error_t err = data->process(data->userdata);

    if (BB_FAILED(err) && data->error)
        data->error(err, data->userdata);

    free(data);
}

static bool client_complete(void *userdata)
{
    _bb_client_task_data_t *data = userdata;
    bb_connection_t *conn = data->client->connection;
    return bb_http_message_complete(conn->buffer, conn->buffer_length);
}

static void client_read_error(bb_error_t err, void *userdata)
{
    _bb_client_task_data_t *data = userdata;

    data->callback(data->client, err, data->userdata);

    bb_client_close(data->client);
    free(data);
}

static bb_error_t client_process(void *userdata)
{
    _bb_client_task_data_t *data = userdata;
    bb_client_t *client = data->client;

    if (bb_response_parse(client->connection->buffer, client->res))
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Response parse failed");
    }

    data->callback(client, BB_SUCCESS(), data->userdata);
    bb_client_close(client);
    free(data);

    return BB_SUCCESS();
}

static void _bb_client_create_read_task(_bb_client_task_data_t *data)
{
    bb_read_task_data_t *read_data = malloc(sizeof(*read_data));
    read_data->connection = data->client->connection;
    read_data->runtime = data->client->runtime;
    read_data->userdata = data;
    read_data->complete = client_complete;
    read_data->error = client_read_error;
    read_data->process = client_process;
    bb_task_t *read = bb_task_create(bb_read_task, read_data);
    bb_runtime_watch_fd(read_data->runtime, read_data->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, read);
}

static bool http_complete(void *userdata)
{
    bb_server_task_data_t *data = userdata;

    return bb_http_message_complete(data->connection->buffer, data->connection->buffer_length);
}

static void http_read_error(bb_error_t err, void *userdata)
{
    (void)err;

    bb_server_task_data_t *data = userdata;

    BB_LOG_ERROR("%s: %s\n", bb_strerror(err.code), err.msg);

    bb_connection_destroy(data->connection);
    free(data);
}

static bb_error_t http_process(void *userdata)
{
    bb_server_task_data_t *data = userdata;
    bb_connection_t *connection = data->connection;

    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    if (bb_request_parse(connection->buffer, req) != 0)
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
            return err;
        }
    }

    bb_response_serialize(res, &connection->write_buffer, &connection->write_length);

    bb_request_destroy(req);
    bb_response_destroy(res);

    connection->write_offset = 0;
    connection->state = BB_CONNECTION_WRITING;

    return _bb_server_create_write_task(data) ? BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") : BB_SUCCESS();
}

static int _bb_http_create_read_task(bb_server_t *server, bb_connection_t *connection, bb_runtime_t *runtime)
{
    bb_server_task_data_t *data = malloc(sizeof(*data));
    if (!data) return 1;

    data->server = server;
    data->connection = connection;
    data->runtime = runtime;
    data->ws_session = NULL;

    bb_read_task_data_t *read_data = malloc(sizeof(*read_data));
    if (!read_data)
    {
        free(data);
        return 1;
    }
    read_data->connection = connection;
    read_data->runtime = runtime;
    read_data->userdata = data;
    read_data->complete = http_complete;
    read_data->error = http_read_error;
    read_data->process = http_process;
    bb_task_t *read = bb_task_create(bb_read_task, read_data);
    bb_runtime_watch_fd(read_data->runtime, read_data->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, read);

    return 0;
}

static void _bb_websocket_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;
    bb_ws_session_t *session = data->ws_session;
    if (bb_connection_read(session->connection) < 0)
    {
        goto cleanup;
    }

    bb_ws_frame_t frame = {0};
    bb_error_t err = bb_websocket_read_frame(session->websocket, &frame);

    /*
     * "Incomplete frame" is NOT a failure, just wait for more data.
     */
    if (err.code == BB_ERR_INTERNAL) // Need better error name
    {
        bb_task_t *next = bb_task_create(_bb_websocket_read_task, data);
        if (!next)
        {
            goto cleanup;
        }

        bb_runtime_watch_fd(data->runtime, session->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
        return;
    }

    if (BB_FAILED(err))
    {
        goto cleanup;
    }

    bb_ws_message_t msg;
    err = bb_ws_frame_to_message(&frame, &msg);
    if (BB_FAILED(err))
    {
        bb_ws_frame_destroy(&frame);
        goto cleanup;
    }
    session->handler(&session->context, &msg);

    if (session->connection->write_buffer)
    {
        if (!data->connection)
        {
            data->connection = data->ws_session->connection;
        }
        if (_bb_server_create_write_task(data) != 0)
        {
            bb_ws_frame_destroy(&frame);
            goto cleanup;
        }

        bb_ws_frame_destroy(&frame);
        return;
    }

    bb_ws_frame_destroy(&frame);

// rearm:
    {
        bb_task_t *next = bb_task_create(_bb_websocket_read_task, data);
        if (!next)
        {
            goto cleanup;
        }
        bb_runtime_watch_fd(data->runtime, session->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
    }
    return;

cleanup:
    bb_ws_session_destroy(session);
    free(data);
}

// Accept:
void bb_accept_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;

    bb_connection_t *connection;
    while ((connection = bb_connection_accept(data->connection->fd)))
    {
        if (_bb_http_create_read_task(data->server, connection, data->runtime) != 0)
        {
            bb_connection_destroy(connection);
        }
    }
}
