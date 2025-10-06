#ifndef NETWORK_H
#define NETWORK_H

#include "core/router.h"

typedef struct {
    int server_fd;
    RouteList *route_list;
} Server;

int init_server(Server *server, int port);
void add_route(Server *server, const char *method, const char *path, RouteHandler handler);
void start_server(Server *server);
void destroy_server(Server *server);

#endif // NETWORK_H
