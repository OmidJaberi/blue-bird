#ifndef BB_SERVER_H
#define BB_SERVER_H

#include "core/router.h"
#include "core/middleware.h"

typedef struct {
    int server_fd;
    route_list_t *route_list;
    middleware_list_t *pre_middleware_list; // Runs before the handler
} bb_server_t;

int init_server(bb_server_t *server, int port);
void add_route(bb_server_t *server, const char *method, const char *path, route_handler_cb handler);
void use_pre_middleware(bb_server_t *server, middleware_cb mw);
void start_server(bb_server_t *server);
void destroy_server(bb_server_t *server);

#endif //BB_SERVER_H
