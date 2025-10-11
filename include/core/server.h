#ifndef NETWORK_H
#define NETWORK_H

#include "core/router.h"
#include "core/middleware.h"

typedef struct {
    int server_fd;
    RouteList *route_list;
    MiddlewareList *middleware_list;
} Server;

int init_server(Server *server, int port);
void add_route(Server *server, const char *method, const char *path, RouteHandler handler);
void use_middleware(Server *server, Middleware mw);
void start_server(Server *server);
void destroy_server(Server *server);

#endif // NETWORK_H
