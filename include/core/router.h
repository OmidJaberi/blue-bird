#ifndef ROUTER_H
#define ROUTER_H

#include "http.h"

#define MAX_ROUTES 50
#define MAX_SEGMENTS 20
#define MAX_PATH_LEN 256

typedef void (*RouteHandler)(Request *req, Response *res);

typedef struct {
    const char *method;
    const char *path;
    RouteHandler handler;
} Route;

typedef struct {
    Route list[MAX_ROUTES];
    int route_count;
} RouteList;

void init_route_list(RouteList *route_list);
void add_route_to_list(RouteList *route_list, const char *method, const char *path, RouteHandler handler);
void handle_request(RouteList *route_list, Request *req, Response *res);

// Just for unit testing, temporaty:
void clear_routes();


#endif // ROUTER_H
