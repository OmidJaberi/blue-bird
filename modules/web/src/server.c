#include "blue-bird/web/server.h"
#include "blue-bird/web/http.h"
#include "blue-bird/web/router.h"
#include "blue-bird/web/middleware.h"
#include "blue-bird/log/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int bb_server_init(bb_server_t *server, int port)
{
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

void bb_server_start(bb_server_t *server)
{
    int client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char *buffer = NULL;

    BB_LOG_INFO("Blue-Bird server started.\n");
    while (1)
    {
        // Accept new connection
        if ((client_fd = accept(server->server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
        {
            BB_LOG_ERROR("accept failed");
            continue;
        }

        // Read request (not parsed, yet)
        bb_http_read_message(client_fd, &buffer);
        BB_LOG_INFO("Received request:\n%s\n", buffer);

        // Parse request
        bb_request_t req;
        bb_response_t res;
        bb_request_init_with_type(&req, BB_SERVER_REQUEST);
        bb_response_init(&res);
        if (bb_request_parse(buffer, &req) == 0)
        {
            if (!BB_FAILED(bb_middleware_list_run(server->pre_middleware_list, &req, &res))) // Pre-Middleware
                bb_route_list_handle_request(server->route_list, &req, &res);
            bb_middleware_list_run(server->post_middleware_list, &req, &res);               // Post-Middleware
        }
        else
        {
            bb_response_set_status(&res, 400);
            bb_response_set_header(&res, "Content-Type", "text/plain");
            bb_response_set_body(&res, "Bad Request");
        }

        bb_response_send(client_fd, &res);
        bb_request_destroy(&req);
        bb_response_destroy(&res);
        close(client_fd);
    }
}

void bb_server_destroy(bb_server_t *server)
{
    bb_route_list_destroy(server->route_list);
    free(server->route_list);
    bb_middleware_list_destroy(server->pre_middleware_list);
    free(server->pre_middleware_list);
    bb_middleware_list_destroy(server->post_middleware_list);
    free(server->post_middleware_list);
}
