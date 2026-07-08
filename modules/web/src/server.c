#include "server_internal.h"
#include "connection/async_tasks.h"
#include "http/parser.h"

#include "blue-bird/runtime/event.h"
#include "blue-bird/log/log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>


typedef struct {
    bb_server_t *server;
    bb_async_connection_t *async_conn;
    bb_websocket_t *ws;
} bb_server_task_data_t;


static bb_error_t default_404(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 404);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Route Not Found");
    return BB_SUCCESS();
}

static bb_error_t default_400(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 400);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Bad Request");
    return BB_SUCCESS();
}

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

    server->async_conn = bb_async_connection_serve(runtime, port);
    if (!server->async_conn)
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

static void _server_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_task_data_t *data = userdata;
    if (data->ws)
    {
        /*
        * Now websocket session owns the connection.
        */
        data->async_conn = NULL;
        bb_error_t err = bb_websocket_create_read_task(data->ws);
        if (BB_FAILED(err))
        {
            bb_websocket_destroy(data->ws);
        }
        free(data);
        return;
    }
    bb_async_connection_destroy(data->async_conn);

    free(data);
}

static void _server_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_task_data_t *data = userdata;
    bb_async_connection_destroy(data->async_conn);
    free(data);
}

static int _server_create_write_task(bb_server_task_data_t *data)
{
    if (BB_FAILED(bb_async_connection_create_write_task(data->async_conn, _server_after_write, _server_write_error, data)))
        return -1;
    return 0;
}

static void _server_read_error(bb_error_t err, void *userdata)
{
    (void)err;

    bb_server_task_data_t *data = userdata;

    BB_LOG_ERROR("%s: %s\n", bb_strerror(err.code), err.msg);

    bb_async_connection_destroy(data->async_conn);
    free(data);
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

static bb_error_t _run_websocket_route(bb_runtime_t *runtime, bb_route_t *route, bb_connection_t *conn, bb_websocket_t **ws, bb_request_t *req, bb_response_t *res)
{
    *ws = bb_websocket_accept(runtime, conn, req, res, bb_route_get_websocket_handler(route));

    if (!(*ws))
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to create websocket.");
    }

    return BB_SUCCESS();
}

bb_error_t _run_request_pipeline(bb_server_t *server, bb_connection_t *conn, bb_websocket_t **ws, bb_request_t *req, bb_response_t *res)
{
    bb_route_t *route = bb_route_list_match(server->route_list, req);
    if (!route)
    {
        return default_404(req, res);
    }

    switch (bb_route_get_type(route))
    {
        case BB_ROUTE_HTTP:
            return _run_http_route(server, route, req, res);
        case BB_ROUTE_WEBSOCKET:
            return _run_websocket_route(server->runtime, route, conn, ws, req, res);
        default:
            return BB_ERROR(BB_ERR_INTERNAL, "Unknown route type");
    }
}

static bb_read_status_t _server_read_step(void *userdata)
{
    bb_server_task_data_t *data = userdata;
    bb_async_connection_t *async_conn = data->async_conn;

    if (!bb_http_message_complete(async_conn->connection->buffer, async_conn->connection->buffer_length))
    {
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
    }

    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    if (bb_request_parse(async_conn->connection->buffer, req))
    {
        default_400(req, res);
    }
    else
    {
        bb_error_t err = _run_request_pipeline(data->server, async_conn->connection, &data->ws, req, res);

        if (BB_FAILED(err))
        {
            bb_request_destroy(req);
            bb_response_destroy(res);

            return (bb_read_status_t){ BB_READ_ERROR, err };
        }
    }

    char *buffer;
    size_t length;
    bb_response_serialize(res, &buffer, &length);
    bb_connection_buffer_add(async_conn->connection, buffer, length);

    bb_request_destroy(req);
    bb_response_destroy(res);

    async_conn->connection->state = BB_CONNECTION_WRITING;

    if (_server_create_write_task(data))
    {
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") };
    }

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

static int _server_create_read_task(bb_server_t *server, bb_async_connection_t *async_conn)
{
    bb_server_task_data_t *data = malloc(sizeof(*data));
    if (!data) return 1;

    data->server = server;
    data->async_conn = async_conn;
    data->ws = NULL;

    if (BB_FAILED(bb_async_connection_create_read_task(async_conn, _server_read_step, _server_read_error, data)))
        return 1;
    return 0;
}

// Accept:
void _server_accept_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;

    bb_async_connection_t *async_conn;
    while ((async_conn = bb_async_connection_accept(data->async_conn->runtime, data->async_conn->connection->fd)))
    {
        if (_server_create_read_task(data->server, async_conn) != 0)
        {
            bb_async_connection_destroy(async_conn);
        }
    }
}

void bb_server_start(bb_server_t *server)
{
    bb_server_task_data_t *data = malloc(sizeof(*data));
    if (!data)
    {
        BB_LOG_ERROR("Failed to allocate server task.\n");
        return;
    }

    data->server = server;
    data->async_conn = server->async_conn;

    bb_task_t *task = bb_task_create(_server_accept_task, data);
    bb_runtime_watch_fd(server->runtime, server->async_conn->connection->fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, task);

    BB_LOG_INFO("Blue-Bird async server started.\n");
}

void bb_server_destroy(bb_server_t *server)
{
    if (!server)
    {
        return;
    }

    if (server->async_conn)
    {
        bb_async_connection_destroy(server->async_conn);
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
