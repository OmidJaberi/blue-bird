#include "blue-bird/web/server.h"
#include "router.h"
#include "middleware.h"
#include "connection.h"
#include "async_connection.h"
#include "websocket/session.h"
#include "server_internal.h"

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

static bb_error_t default_404(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 404);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Route Not Found");
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

void bb_server_start(bb_server_t *server)
{
    _bb_accept_task_data_t *data = malloc(sizeof(*data));

    if (!data)
    {
        return;
    }

    data->server = server;
    data->runtime = server->runtime;
    data->connection = server->connection;

    bb_task_t *task = bb_task_create(bb_accept_task, data);

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

static bb_error_t _run_websocket_route(bb_route_t *route, bb_ws_session_t **session, bb_request_t *req, bb_response_t *res)
{
    BB_LOG_INFO("Starting websocket upgrade\n");

    bb_error_t err = bb_websocket_accept(req, res);
    printf("1\n");
    (*session)->connection->buffer_length = 0; // Temporary Buffer reset
    printf("2\n");

    if (BB_FAILED(err))
    {
        BB_LOG_ERROR("Upgrade failed: %s\n", err.msg);
        return err;
    }

    BB_LOG_INFO("Upgrade accepted\n");

    *session = bb_ws_session_create((*session)->connection, bb_route_get_websocket_handler(route));

    if (!(*session))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to create websocket session");
    }

    return BB_SUCCESS();
}

bb_error_t bb_server_run_request_pipeline(bb_server_t *server, bb_ws_session_t **session, bb_request_t *req, bb_response_t *res)
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
            return _run_websocket_route(route, session, req, res);
        default:
            return BB_ERROR(BB_ERR_INTERNAL, "Unknown route type");
    }
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
