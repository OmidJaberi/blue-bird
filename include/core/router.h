#ifndef ROUTER_H
#define ROUTER_H

#include "http.h"

typedef void (*RouteHandler)(Request *req, Response *res);

typedef struct {
    const char *method;
    const char *path;
    RouteHandler handler;
} Route;

void add_route(const char *method, const char *path, RouteHandler handler);
void handle_request(Request *req, Response *res);

#endif // ROUTER_H
