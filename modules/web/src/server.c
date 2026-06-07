#include "blue-bird/web/server.h"
#include "http/parser.h"
#include "router.h"
#include "middleware.h"
#include "connection.h"

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
} _bb_client_task_data_t;

bb_server_t *bb_server_create_on_runtime(bb_runtime_t *runtime, int port)
{
    bb_server_t *server = malloc(sizeof(bb_server_t));
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

bb_server_t *bb_server_create(int port)
{
    bb_runtime_t *runtime = bb_runtime_create();
    
    if (!runtime)
    {
        return NULL;
    }

    return bb_server_create_on_runtime(runtime, port);
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

static bb_error_t _run_request_pipeline(bb_server_t *server, bb_request_t *req, bb_response_t *res)
{
    bb_error_t err;

    err = bb_middleware_list_run(server->pre_middleware_list, req, res);
    if (BB_FAILED(err))
        return err;

    bb_route_t *route = bb_route_list_match(server->route_list, req);
    bb_http_handler_cb handler = route ? bb_route_get_handler(route) : default_404;

    err = handler(req, res);
    if (BB_FAILED(err))
        return err;

    return bb_middleware_list_run(server->post_middleware_list, req, res);
}

static void _bb_client_write_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_client_task_data_t *data = userdata;

    bb_connection_t *connection = data->connection;
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
        bb_connection_destroy(connection);
        free(data);
        return;
    }

    /*
     * Request incomplete:
     * re-arm READ watcher
     */
    if (!bb_http_request_complete(connection->buffer, connection->buffer_length))
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

    // Parse request
    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();
    if (bb_request_parse(connection->buffer, req) != 0)
    {
        default_400(req, res);
    }
    else
    {
        bb_error_t err = _run_request_pipeline(server, req, res);
        if (BB_FAILED(err))
        {
            BB_LOG_ERROR("%s: %s\n", bb_strerror(err.code), err.msg);
        }
    }

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
    bb_runtime_run(server->runtime); //temp
}

void bb_server_destroy(bb_server_t *server)
{
    bb_route_list_destroy(server->route_list);
    bb_middleware_list_destroy(server->pre_middleware_list);
    bb_middleware_list_destroy(server->post_middleware_list);
    bb_runtime_destroy(server->runtime); // temp
}

void bb_server_add_route(bb_server_t *server, const char *method, const char *path, bb_http_handler_cb handler)
{
    bb_route_list_add(server->route_list, method, path, handler);
}

void bb_server_use_pre_middleware(bb_server_t *server, bb_http_handler_cb mw)
{
    bb_middleware_list_append(server->pre_middleware_list, mw);
}

void bb_server_use_post_middleware(bb_server_t *server, bb_http_handler_cb mw)
{
    bb_middleware_list_append(server->post_middleware_list, mw);
}
