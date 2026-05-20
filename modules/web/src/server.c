#include "blue-bird/web/server.h"
#include "blue-bird/web/http.h"
#include "blue-bird/web/router.h"
#include "blue-bird/web/middleware.h"
#include "blue-bird/web/connection.h"

#include "blue-bird/runtime/event.h"

#include "blue-bird/log/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

typedef struct {
    bb_server_t *server;
} _bb_accept_task_data_t;

typedef struct {
    bb_connection_t *connection;
} _bb_client_task_data_t;

static int _bb_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int bb_server_init_on_runtime(bb_server_t *server, bb_runtime_t *runtime, int port)
{
    server->runtime = runtime;

    server->route_list = (bb_route_list_t *)malloc(sizeof(bb_route_list_t));
    bb_route_list_init(server->route_list);

    server->pre_middleware_list = (bb_middleware_list_t *)malloc(sizeof(bb_middleware_list_t));
    bb_middleware_list_init(server->pre_middleware_list);

    server->post_middleware_list = (bb_middleware_list_t *)malloc(sizeof(bb_middleware_list_t));
    bb_middleware_list_init(server->post_middleware_list);

    struct sockaddr_in address;
    int opt = 1;

    // Create socket
    if ((server->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        BB_LOG_ERROR("socket failed");
        exit(EXIT_FAILURE);
    }

    _bb_set_nonblocking(server->server_fd);

    // Reuse port
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        BB_LOG_ERROR("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind
    if (bind(server->server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        BB_LOG_ERROR("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server->server_fd, 3) < 0)
    {
        BB_LOG_ERROR("listen failed");
        exit(EXIT_FAILURE);
    }

    BB_LOG_INFO("Blue-Bird server initialized on port %d\n", port);
    return 0;
}

int bb_server_init(bb_server_t *server, int port)
{
    bb_runtime_t *runtime = bb_runtime_create();
    
    if (!runtime)
    {
        return -1;
    }

    bb_server_init_on_runtime(server, runtime, port);
    return 0;
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
    bb_http_handler_cb handler = route ? route->handler : default_404;

    err = handler(req, res);
    if (BB_FAILED(err))
        return err;

    return bb_middleware_list_run(server->post_middleware_list, req, res);
}

static void _bb_client_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_client_task_data_t *data = userdata;

    bb_connection_t *connection = data->connection;

    bb_server_t *server = connection->server;

    if (bb_http_read_message(connection->client_fd, &connection->buffer) <= 0)
    {
        bb_connection_destroy(connection);
        free(data);
        return;
    }

    if (bb_request_parse(connection->buffer, &connection->request) != 0)
    {
        default_400(&connection->request, &connection->response);
    }
    else
    {
        _run_request_pipeline(server, &connection->request, &connection->response);
    }

    bb_response_send(connection->client_fd, &connection->response);

    bb_connection_destroy(connection);

    free(data);
}

static void _bb_accept_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_accept_task_data_t *data = userdata;

    bb_server_t *server = data->server;

    struct sockaddr_in address;

    socklen_t addrlen = sizeof(address);

    while (1)
    {
        int client_fd = accept(server->server_fd, (struct sockaddr *)&address, &addrlen);

        if (client_fd < 0)
        {
            /*
             * Nonblocking socket:
             * no more pending connections.
             */
            break;
        }

        _bb_set_nonblocking(client_fd);

        bb_connection_t *connection = bb_connection_create(server, client_fd);

        if (!connection)
        {
            close(client_fd);
            continue;
        }

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

        bb_task_t *client_task = bb_task_create(_bb_client_read_task, client_data);

        if (!client_task)
        {
            bb_connection_destroy(connection);
            free(client_data);
            continue;
        }
        bb_runtime_watch_fd(server->runtime, client_fd, BB_EVENT_READ, BB_WATCH_ONESHOT, client_task);
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

    bb_runtime_watch_fd(server->runtime, server->server_fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, task);

    BB_LOG_INFO("Blue-Bird async server started.\n");
    bb_runtime_run(server->runtime); //temp
}

void bb_server_destroy(bb_server_t *server)
{
    bb_route_list_destroy(server->route_list);
    free(server->route_list);
    bb_middleware_list_destroy(server->pre_middleware_list);
    free(server->pre_middleware_list);
    bb_middleware_list_destroy(server->post_middleware_list);
    free(server->post_middleware_list);
    bb_runtime_destroy(server->runtime); // temp
}

void bb_server_add_route(bb_server_t *server, const char *method, const char *path, bb_route_handler_cb handler)
{
    bb_route_list_add(server->route_list, method, path, handler);
}

void bb_server_use_pre_middleware(bb_server_t *server, bb_middleware_cb mw)
{
    bb_middleware_list_append(server->pre_middleware_list, mw);
}

void bb_server_use_post_middleware(bb_server_t *server, bb_middleware_cb mw)
{
    bb_middleware_list_append(server->post_middleware_list, mw);
}
