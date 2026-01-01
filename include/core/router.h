#ifndef ROUTER_H
#define ROUTER_H

#include "http.h"
#include "error/error.h"

#define MAX_ROUTES 50
#define MAX_SEGMENTS 20
#define MAX_PATH_LEN 256

typedef http_handler_cb route_handler_cb;

typedef struct {
    const char *method;
    const char *path;
    route_handler_cb handler;
} Route;

typedef struct {
    Route list[MAX_ROUTES];
    int route_count;
} RouteList;

void init_route_list(RouteList *route_list);
BBError add_route_to_list(RouteList *route_list, const char *method, const char *path, route_handler_cb handler);
void handle_request(RouteList *route_list, request_t *req, response_t *res);

#endif // ROUTER_H
