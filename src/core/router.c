#include "core/router.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_ROUTES 50

static Route routes[MAX_ROUTES];
static int route_count = 0;

void add_route(const char *path, RouteHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        fprintf(stderr, "Max routes reached!\\n");
        return;
    }
    routes[route_count].path = path;
    routes[route_count].handler = handler;
    route_count++;
}

void handle_request(Request *req, Response *res)
{
    for (int i = 0; i < route_count; i++)
    {
        if (strcmp(req->path, routes[i].path) == 0)
        {
            routes[i].handler(req, res);
            return;
        }
    }

    // Default 404
    init_response(res);
    set_status(res, 404);
    set_header(res, "Content-Type", "text/plain");
    //set_header(res, "Content-Length", "0");
    set_body(res, "Route Not Found");
}
