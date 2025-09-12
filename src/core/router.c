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

void handle_request(Request *req, int client_fd)
{
    for (int i = 0; i < route_count; i++)
    {
        if (strcmp(req->path, routes[i].path) == 0)
        {
            routes[i].handler(req, client_fd);
            return;
        }
    }

    // Default 404
    char *not_found = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 9\r\n"
                      "\r\n"
                      "Not Found";
    write(client_fd, not_found, strlen(not_found));
}
