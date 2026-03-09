#ifndef BB_ROUTER_H
#define BB_ROUTER_H

#include "http.h"
#include "error/error.h"

#define MAX_SEGMENTS 20
#define MAX_PATH_LEN 256

typedef http_handler_cb route_handler_cb;

typedef struct Route {
    const char *method;
    const char *path;
    route_handler_cb handler;
    struct Route *next_route;
} route_t;

typedef struct {
    route_t *first;
} route_list_t;

void init_route_list(route_list_t *route_list);
BBError add_route_to_list(route_list_t *route_list, const char *method, const char *path, route_handler_cb handler);
void handle_request(route_list_t *route_list, request_t *req, response_t *res);

#endif //BB_ROUTER_H
