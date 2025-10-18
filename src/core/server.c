#include "core/server.h"
#include "core/http.h"
#include "core/router.h"
#include "core/middleware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int init_server(Server *server, int port)
{
    server->route_list = (RouteList *)malloc(sizeof(RouteList));
    init_route_list(server->route_list);

    server->middleware_list = (MiddlewareList *)malloc(sizeof(MiddlewareList));
    init_middleware_list(server->middleware_list);

    struct sockaddr_in address;
    int opt = 1;

    // Create socket
    if ((server->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Reuse port
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind
    if (bind(server->server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server->server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Blue-Bird server initialized on port %d\n", port);
    return 0;
}

void add_route(Server *server, const char *method, const char *path, RouteHandler handler)
{
    add_route_to_list(server->route_list, method, path, handler);
}

void use_middleware(Server *server, Middleware mw)
{
    append_to_middleware_list(server->middleware_list, mw);
}

void start_server(Server *server)
{
    int client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[3000] = {0};

    printf("Blue-Bird server started.\n");
    while (1)
    {
        // Accept new connection
        if ((client_fd = accept(server->server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
        {
            perror("accept failed");
            continue;
        }

        // Read request (not parsed, yet)
        read(client_fd, buffer, sizeof(buffer));
        printf("Received request:\n%s\n", buffer);

        // Parse request
        Request req;
        Response res;
        init_response(&res);
        if (parse_request(buffer, &req) == 0)
        {
            if (!BB_FAILED(run_middleware(server->middleware_list, &req, &res)))
                handle_request(server->route_list, &req, &res);
        }
        else
        {
            init_response(&res);
            set_status(&res, 400);
            set_header(&res, "Content-Type", "text/plain");
            set_body(&res, "Bad Request");
        }

        send_response(client_fd, &res);
        destroy_request(&req);
        destroy_response(&res);
        close(client_fd);
    }
}

void destroy_server(Server *server)
{
    free(server->route_list);
    destroy_middleware_list(server->middleware_list);
    free(server->middleware_list);
}
