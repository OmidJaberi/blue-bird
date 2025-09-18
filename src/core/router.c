#include "core/router.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_ROUTES 50
#define MAX_SEGMENTS 20
#define MAX_PATH_LEN 256

static Route routes[MAX_ROUTES];
static int route_count = 0;

static int split_path(const char *path, char segments[MAX_SEGMENTS][MAX_PATH_LEN])
{
    int count = 0;
    const char *start = path;
    const char *p;
    
    for (p = path; *p && count < MAX_SEGMENTS; p++)
    {
        if (*p == '/')
        {
            if (p > start)
            {
                size_t len = p - start;
                strncpy(segments[count], start, len);
                segments[count][len] = '\0';
                count++;
            }
            start = p + 1;
        }
    }

    if (p > start && count < MAX_SEGMENTS)
    {
        size_t len = p - start;
        strncpy(segments[count], start, len);
        segments[count][len] = '\0';
        count++;
    }

    return count;
}

void add_route(const char *method, const char *path, RouteHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        fprintf(stderr, "Max routes reached!\\n");
        return;
    }
    routes[route_count].method = method;
    routes[route_count].path = path;
    routes[route_count].handler = handler;
    route_count++;
}

void handle_request(Request *req, Response *res)
{
    char req_segments[MAX_SEGMENTS][MAX_PATH_LEN];
    int req_count = split_path(req->path, req_segments);

    for (int i = 0; i < route_count; i++)
    {
        if (strcmp(req->method, routes[i].method) != 0) continue;

        char route_segments[MAX_SEGMENTS][MAX_PATH_LEN];
        int route_count = split_path(routes[i].path, route_segments);

        if (req_count != route_count) continue;

        req->param_count = 0;
        int match = 1;

        for (int j = 0; j < req_count; j++)
        {
            if (route_segments[j][0] == ':')
            {
                // Parameter
                if (req->param_count >= MAX_PARAMS)
                    continue;
                strncpy(req->params[req->param_count].name, route_segments[j] + 1, MAX_PARAM_NAME - 1);
                strncpy(req->params[req->param_count].value, req_segments[j], MAX_PARAM_VALUE - 1);

                req->params[req->param_count].name[MAX_PARAM_NAME - 1] = '\0';
                req->params[req->param_count].value[MAX_PARAM_VALUE - 1] = '\0';

                req->param_count++;
            }
            else
            {
                // Exact match
                if (strcmp(route_segments[j], req_segments[j]) != 0)
                {
                    match = 0;
                    break;
                }
            }
        }

        if (match)
        {
            routes[i].handler(req, res);
            return;
        }
    }

    // Default 404
    init_response(res);
    set_status(res, 404);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Route Not Found");
}

// Just for unit testing, temporaty:
void clear_routes()
{
    route_count = 0;
}
