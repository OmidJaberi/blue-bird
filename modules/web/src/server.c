#include "blue-bird/web/server.h"
#include "http/parser.h"
#include "router.h"
#include "middleware.h"
#include "connection.h"
#include "websocket/context_internal.h"
#include "websocket/websocket_internal.h"
#include "websocket/session.h"

#include "blue-bird/runtime/event.h"
#include "blue-bird/log/log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct bb_server {
    bb_connection_t *connection;
    bb_runtime_t *runtime;
    bb_route_list_t *route_list;
    bb_middleware_list_t *pre_middleware_list; // Runs before the handler
    bb_middleware_list_t *post_middleware_list; // Runs after the handler
};

typedef struct {
    bb_server_t *server;
} _bb_accept_task_data_t;

typedef struct {
    bb_connection_t *connection;
    bb_server_t *server;
    bb_ws_session_t *ws_session;
} _bb_client_task_data_t;

bb_server_t *bb_server_create_on_runtime(bb_runtime_t *runtime, int port)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_server_t *server = calloc(1, sizeof(*server));
    if (!server)
    {
        return NULL;
    }

    server->runtime = runtime;

    server->connection = bb_connection_serve(port);
    if (!server->connection)
    {
        bb_server_destroy(server);
        return NULL;
    }

    server->route_list = bb_route_list_create();
    server->pre_middleware_list = bb_middleware_list_create();
    server->post_middleware_list = bb_middleware_list_create();

    BB_LOG_INFO("Blue-Bird server initialized on port %d\n", port);
    return server;
}

static bb_error_t default_400(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 400);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Bad Request");
    return BB_SUCCESS();
}

static bb_error_t default_404(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 404);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Route Not Found");
    return BB_SUCCESS();
}

static bb_error_t _run_http_route(bb_server_t *server, bb_route_t *route, bb_request_t *req, bb_response_t *res)
{
    bb_error_t err;

    err = bb_middleware_list_run(server->pre_middleware_list, req, res);

    if (BB_FAILED(err))
    {
        return err;
    }

    err = bb_route_get_http_handler(route)(req, res);

    if (BB_FAILED(err))
    {
        return err;
    }

    return bb_middleware_list_run(server->post_middleware_list, req, res);
}

static bb_error_t _run_websocket_route(bb_route_t *route, _bb_client_task_data_t *client, bb_request_t *req, bb_response_t *res)
{
    BB_LOG_INFO("Starting websocket upgrade\n");

    bb_error_t err = bb_websocket_accept(req, res);
    client->connection->buffer_length = 0; // Temporary Buffer reset

    if (BB_FAILED(err))
    {
        BB_LOG_ERROR("Upgrade failed: %s\n", err.msg);
        return err;
    }

    BB_LOG_INFO("Upgrade accepted\n");

    client->ws_session = bb_ws_session_create(client->connection, bb_route_get_websocket_handler(route));

    if (!client->ws_session)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to create websocket session");
    }

    /*
    * Session now owns the connection.
    */
    // client->connection = NULL;

    return BB_SUCCESS();
}

static bb_error_t _run_request_pipeline(bb_server_t *server, _bb_client_task_data_t *client, bb_request_t *req, bb_response_t *res)
{
    BB_LOG_INFO("Looking for route\n");
    bb_route_t *route = bb_route_list_match(server->route_list, req);


    if (!route)
    {
        BB_LOG_INFO("Route not found\n");
        return default_404(req, res);
    }

    BB_LOG_INFO("Route found\n");

    switch (bb_route_get_type(route))
    {
        case BB_ROUTE_HTTP:
            return _run_http_route(server, route, req, res);
        case BB_ROUTE_WEBSOCKET:
            return _run_websocket_route(route, client, req, res);
        default:
            return BB_ERROR(BB_ERR_INTERNAL, "Unknown route type");
    }
}

static void _bb_client_write_task(bb_task_t *task, void *userdata);

static void _bb_websocket_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_client_task_data_t *data = userdata;
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

        bb_runtime_watch_fd(data->server->runtime, session->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
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
        bb_task_t *write_task = bb_task_create(_bb_client_write_task, data);

        if (!write_task)
        {
            bb_ws_frame_destroy(&frame);
            goto cleanup;
        }

        bb_runtime_watch_fd(data->server->runtime, session->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, write_task);

        bb_ws_frame_destroy(&frame);
        return;
    }

    bb_ws_frame_destroy(&frame);

rearm:
    {
        bb_task_t *next = bb_task_create(_bb_websocket_read_task, data);
        if (!next)
        {
            goto cleanup;
        }
        bb_runtime_watch_fd(data->server->runtime, session->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
    }
    return;

cleanup:
    bb_ws_session_destroy(session);
    free(data);
}

static void _bb_client_write_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_client_task_data_t *data = userdata;

    bb_connection_t *connection = data->connection ? data->connection : data->ws_session->connection;
    bb_server_t *server = data->server;

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
        bb_task_t *write_task = bb_task_create(_bb_client_write_task, data);

        if (!write_task)
        {
            bb_connection_destroy(connection);
            free(data);
            return;
        }

        bb_runtime_watch_fd(
            server->runtime,
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
        bb_runtime_watch_fd(server->runtime, data->ws_session->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, task);
        return;
    }
    bb_connection_destroy(connection);

    free(data);
}

static void _bb_client_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_client_task_data_t *data = userdata;

    bb_connection_t *connection = data->connection;

    bb_server_t *server = data->server;

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
        bb_task_t *read_task = bb_task_create(_bb_client_read_task, data);

        if (!read_task)
        {
            bb_connection_destroy(connection);
            free(data);
            return;
        }

        bb_runtime_watch_fd(
            server->runtime,
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
        bb_error_t err = _run_request_pipeline(server, data, req, res);
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
    bb_task_t *write_task = bb_task_create(_bb_client_write_task, data);

    if (!write_task)
    {
        bb_connection_destroy(connection);
        free(data);
        return;
    }

    bb_runtime_watch_fd(
        server->runtime,
        connection->fd,
        BB_EVENT_WRITE,
        BB_WATCH_ONESHOT,
        write_task
    );
}

static void _bb_accept_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_accept_task_data_t *data = userdata;

    bb_server_t *server = data->server;

    bb_connection_t *connection;
    while ((connection = bb_connection_accept(server->connection->fd)))
    {
        /*
         * Register client watcher
         */
        _bb_client_task_data_t *client_data = malloc(sizeof(*client_data));

        if (!client_data)
        {
            bb_connection_destroy(connection);
            continue;
        }

        client_data->connection = connection;
        client_data->server = server;
        client_data->ws_session = NULL;

        bb_task_t *client_task = bb_task_create(_bb_client_read_task, client_data);

        if (!client_task)
        {
            bb_connection_destroy(connection);
            free(client_data);
            continue;
        }
        bb_runtime_watch_fd(server->runtime, connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, client_task);
    }
}

void bb_server_start(bb_server_t *server)
{
    _bb_accept_task_data_t *data = malloc(sizeof(*data));

    if (!data)
    {
        return;
    }

    data->server = server;

    bb_task_t *task = bb_task_create(_bb_accept_task, data);

    bb_runtime_watch_fd(server->runtime, server->connection->fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, task);

    BB_LOG_INFO("Blue-Bird async server started.\n");
}

void bb_server_destroy(bb_server_t *server)
{
    if (!server)
    {
        return;
    }

    if (server->connection)
    {
        bb_connection_destroy(server->connection);
    }

    bb_route_list_destroy(server->route_list);
    bb_middleware_list_destroy(server->pre_middleware_list);
    bb_middleware_list_destroy(server->post_middleware_list);

    free(server);
}

void bb_server_add_route(bb_server_t *server, const char *method, const char *path, bb_http_handler_cb handler)
{
    bb_route_list_add_http(server->route_list, method, path, handler);
}

void bb_server_add_websocket(bb_server_t *server, const char *path, bb_ws_handler_cb handler)
{
    bb_route_list_add_websocket(server->route_list, path, handler);
}

void bb_server_use_pre_middleware(bb_server_t *server, bb_http_handler_cb mw)
{
    bb_middleware_list_append(server->pre_middleware_list, mw);
}

void bb_server_use_post_middleware(bb_server_t *server, bb_http_handler_cb mw)
{
    bb_middleware_list_append(server->post_middleware_list, mw);
}
