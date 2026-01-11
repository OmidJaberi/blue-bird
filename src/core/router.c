#include "core/router.h"
#include "error/assert.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

void init_route_list(RouteList *route_list)
{
    route_list->route_count = 0;
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

BBError add_route_to_list(RouteList *route_list, const char *method, const char *path, route_handler_cb handler)
{
    // Basic sanity checks
    BB_ASSERT(route_list != NULL, "Route list pointer is NULL");
    BB_ASSERT(method != NULL, "HTTP method is NULL");
    BB_ASSERT(path != NULL, "Route path is NULL");
    BB_ASSERT(handler != NULL, "Route handler is NULL");

    // Runtime validation
    if (route_list->route_count >= MAX_ROUTES)
        return BB_ERROR(BB_ERR_INTERNAL, "Maximum number of routes reached.");

    // Add the route
    route_list->list[route_list->route_count].method = method;
    route_list->list[route_list->route_count].path = path;
    route_list->list[route_list->route_count].handler = handler;
    route_list->route_count++;

    return BB_SUCCESS();
}

void handle_request(RouteList *route_list, request_t *req, response_t *res)
{
    char req_segments[MAX_SEGMENTS][MAX_PATH_LEN];
    int req_count = split_path(GET_REQUEST_PATH(*req), req_segments);

    for (int i = 0; i < route_list->route_count; i++)
    {
        if (strcmp(GET_REQUEST_METHOD(*req), route_list->list[i].method) != 0) continue;

        char route_segments[MAX_SEGMENTS][MAX_PATH_LEN];
        int route_segment_count = split_path(route_list->list[i].path, route_segments);

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
            route_list->list[i].handler(req, res);
            return;
        }
    }

    // Default 404
    init_response(res);
    set_response_status(res, 404);
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Route Not Found");
}
