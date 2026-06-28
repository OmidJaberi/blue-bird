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
static void _bb_client_read_task(bb_task_t *task, void *userdata);

static void client_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    _bb_client_task_data_t *client = userdata;
    bb_task_t *read = bb_task_create(_bb_client_read_task, client);
    bb_runtime_watch_fd(client->client->runtime, client->client->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, read);
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

    if (!client->connection->write_buffer)
    {
        bb_request_serialize(client->req, &client->connection->write_buffer, &client->connection->write_length);
        client->connection->write_offset = 0;
    }

    bb_task_t *task = bb_task_create(bb_write_task, write);
    bb_runtime_watch_fd(client->runtime, client->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
}

static void _bb_client_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_client_task_data_t *data = userdata;
    bb_client_t *client = data->client;
    bb_connection_t *conn = client->connection;

    ssize_t n = bb_connection_read(conn);
    if (n < 0)
    {
        data->callback(client, BB_ERROR(BB_ERR_IO, "Read failed"), data->userdata);
        bb_client_close(client);
        free(data);
        return;
    }

    if (!bb_http_message_complete(conn->buffer, conn->buffer_length))
    {
        bb_task_t *next = bb_task_create(_bb_client_read_task, data);
        bb_runtime_watch_fd(client->runtime, conn->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
        return;
    }

    if (bb_response_parse(conn->buffer, client->res) != 0)
    {
        data->callback(client, BB_ERROR(BB_ERR_INTERNAL, "Response parse failed"), data->userdata);
        bb_client_close(client);
        free(data);
        return;
    }

    data->callback(client, BB_SUCCESS(), data->userdata);
    bb_client_close(client);
    free(data);
}

static void _bb_server_write_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;

    bb_connection_t *connection = data->connection;

    ssize_t rc = bb_connection_write(connection);

    // Fatal socket error
    if (rc < 0)
    {
        bb_connection_destroy(connection);
        free(data);
        return;
    }

    /*
     * Partial write:
     * wait for next writable event
     */
    if (connection->write_offset < connection->write_length)
    {
        bb_task_t *write_task = bb_task_create(_bb_server_write_task, data);

        if (!write_task)
        {
            bb_connection_destroy(connection);
            free(data);
            return;
        }

        bb_runtime_watch_fd(
            data->runtime,
            connection->fd,
            BB_EVENT_WRITE,
            BB_WATCH_ONESHOT,
            write_task
        );

        return;
    }

    /*
     * Response fully written
     */
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
    bb_connection_destroy(connection);

    free(data);
}

static void _bb_http_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;

    bb_connection_t *connection = data->connection;

    if (bb_connection_read(connection) < 0)
    {
        BB_LOG_INFO("read failed\n");
        bb_connection_destroy(connection);
        free(data);
        return;
    }
    BB_LOG_INFO("buffer:\n%.*s\n", (int)connection->buffer_length, connection->buffer);

    /*
     * Request incomplete:
     * re-arm READ watcher
     */
    if (!bb_http_message_complete(connection->buffer, connection->buffer_length))
    {
        bb_task_t *read_task = bb_task_create(_bb_http_read_task, data);

        if (!read_task)
        {
            bb_connection_destroy(connection);
            free(data);
            return;
        }

        bb_runtime_watch_fd(
            data->runtime,
            connection->fd,
            BB_EVENT_READ,
            BB_WATCH_ONESHOT,
            read_task
        );

        return;
    }
    BB_LOG_INFO("HTTP message complete\n");

    // Parse request
    BB_LOG_INFO("Parsing request\n");
    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();
    if (bb_request_parse(connection->buffer, req) != 0)
    {
        default_400(req, res);
    }
    else
    {
        bb_error_t err = bb_server_run_request_pipeline(data->server, data->connection, &(data->ws_session), req, res);
        if (BB_FAILED(err))
        {
            BB_LOG_ERROR("%s: %s\n", bb_strerror(err.code), err.msg);
        }
    }
    BB_LOG_INFO("Method=%s Path=%s\n", bb_request_get_method(req), bb_request_get_path(req));

    // Serialize response
    bb_response_serialize(
        res,
        &connection->write_buffer,
        &connection->write_length
    );

    bb_request_destroy(req);
    bb_response_destroy(res);

    connection->write_offset = 0;

    connection->state = BB_CONNECTION_WRITING;

    // Register WRITE watcher
    bb_task_t *write_task = bb_task_create(_bb_server_write_task, data);

    if (!write_task)
    {
        bb_connection_destroy(connection);
        free(data);
        return;
    }

    bb_runtime_watch_fd(
        data->runtime,
        connection->fd,
        BB_EVENT_WRITE,
        BB_WATCH_ONESHOT,
        write_task
    );
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
        bb_task_t *write_task = bb_task_create(_bb_server_write_task, data);

        if (!write_task)
        {
            bb_ws_frame_destroy(&frame);
            goto cleanup;
        }

        bb_runtime_watch_fd(data->runtime, session->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, write_task);

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

void bb_accept_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;

    bb_connection_t *connection;
    while ((connection = bb_connection_accept(data->connection->fd)))
    {
        /*
         * Register client watcher
         */
        bb_server_task_data_t *client_data = malloc(sizeof(*client_data));

        if (!client_data)
        {
            bb_connection_destroy(connection);
            continue;
        }

        client_data->server = data->server;
        client_data->connection = connection;
        client_data->runtime = data->runtime;
        client_data->ws_session = NULL;

        bb_task_t *client_task = bb_task_create(_bb_http_read_task, client_data);

        if (!client_task)
        {
            bb_connection_destroy(connection);
            free(client_data);
            continue;
        }
        bb_runtime_watch_fd(data->runtime, connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, client_task);
    }
}
