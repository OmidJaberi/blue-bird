#include "core/router.h"
#include "error/assert.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

void init_route_list(route_list_t *route_list)
{
    route_list->first = NULL;
}

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

BBError add_route_to_list(route_list_t *route_list, const char *method, const char *path, route_handler_cb handler)
{
    // Basic sanity checks
    BB_ASSERT(route_list != NULL, "Route list pointer is NULL");
    BB_ASSERT(method != NULL, "HTTP method is NULL");
    BB_ASSERT(path != NULL, "Route path is NULL");
    BB_ASSERT(handler != NULL, "Route handler is NULL");

    route_t *new_route = malloc(sizeof(route_t));
    
    // Add the route
    new_route->method = method;
    new_route->path = path;
    new_route->handler = handler;
    
    new_route->next_route = route_list->first;
    route_list->first = new_route;

    return BB_SUCCESS();
}

void handle_request(route_list_t *route_list, request_t *req, response_t *res)
{
    char req_segments[MAX_SEGMENTS][MAX_PATH_LEN];
    int req_count = split_path(GET_REQUEST_PATH(*req), req_segments);

    for (route_t *route = route_list->first; route != NULL; route = route->next_route)
    {
        if (strcmp(GET_REQUEST_METHOD(*req), route->method) != 0) continue;

        char route_segments[MAX_SEGMENTS][MAX_PATH_LEN];
        int route_segment_count = split_path(route->path, route_segments);

        if (req_count != route_segment_count) continue;

        GET_REQUEST_PARAM_COUNT(*req) = 0;
        int match = 1;

        for (int j = 0; j < req_count; j++)
        {
            if (route_segments[j][0] == ':')
            {
                // Parameter
                if (GET_REQUEST_PARAM_COUNT(*req) >= MAX_PARAMS)
                    continue;
                strncpy(GET_REQUEST_PARAMS(*req)[GET_REQUEST_PARAM_COUNT(*req)].name, route_segments[j] + 1, MAX_PARAM_NAME - 1);
                strncpy(GET_REQUEST_PARAMS(*req)[GET_REQUEST_PARAM_COUNT(*req)].value, req_segments[j], MAX_PARAM_VALUE - 1);

                GET_REQUEST_PARAMS(*req)[GET_REQUEST_PARAM_COUNT(*req)].name[MAX_PARAM_NAME - 1] = '\0';
                GET_REQUEST_PARAMS(*req)[GET_REQUEST_PARAM_COUNT(*req)].value[MAX_PARAM_VALUE - 1] = '\0';

                GET_REQUEST_PARAM_COUNT(*req)++;
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
            route->handler(req, res);
            return;
        }
    }

    // Default 404
    init_response(res);
    set_response_status(res, 404);
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Route Not Found");
}

void destroy_route_list(route_list_t *route_list)
{
    route_t *current = route_list->first;
    while (current)
    {
        route_t *next = current->next_route;
        free(current->method);
        free(current->path);
        free(current);
        current = next;
    }
}
