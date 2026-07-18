#include "router.h"
#include "blue-bird/error/assert.h"
#include <string.h>
#include <stdio.h>

#define MAX_SEGMENTS 20
#define MAX_PATH_LEN 256

struct bb_route {
    bb_route_type_t type;
    char *method;
    char path_segments[MAX_SEGMENTS][MAX_PATH_LEN];
    int segments_count;

    union {
        bb_http_handler_cb http_handler;
        bb_ws_handler_cb websocket_handler;
    };
    bb_route_t *next_route;
};

bb_route_type_t bb_route_get_type(bb_route_t *route)
{
    return route->type;
}

bb_http_handler_cb bb_route_get_http_handler(bb_route_t *route)
{
    return route->http_handler;
}

bb_ws_handler_cb bb_route_get_websocket_handler(bb_route_t *route)
{
    return route->websocket_handler;
}

bb_route_list_t *bb_route_list_create(void)
{
    bb_route_list_t *route_list = calloc(1, sizeof(*route_list));
    return route_list;
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

bb_error_t bb_route_list_add_http(bb_route_list_t *route_list, const char *method, const char *path, bb_http_handler_cb handler)
{
    // Basic sanity checks
    BB_ASSERT_MSG(route_list != NULL, "Route list pointer is NULL");
    BB_ASSERT_MSG(method != NULL, "HTTP method is NULL");
    BB_ASSERT_MSG(path != NULL, "Route path is NULL");
    BB_ASSERT_MSG(handler != NULL, "Route handler is NULL");

    bb_route_t *new_route = malloc(sizeof(bb_route_t));
    
    // Add the route
    new_route->type = BB_ROUTE_HTTP;
    new_route->method = strdup(method);
    new_route->segments_count = split_path(path, new_route->path_segments);
    new_route->http_handler = handler;
    
    new_route->next_route = *route_list;
    *route_list = new_route;

    return BB_SUCCESS();
}

bb_error_t bb_route_list_add_websocket(bb_route_list_t *route_list, const char *path, bb_ws_handler_cb handler)
{
    // Basic sanity checks
    BB_ASSERT_MSG(route_list != NULL, "Route list pointer is NULL");
    BB_ASSERT_MSG(path != NULL, "Route path is NULL");
    BB_ASSERT_MSG(handler != NULL, "Route handler is NULL");

    bb_route_t *new_route = malloc(sizeof(bb_route_t));
    
    // Add the route
    new_route->type = BB_ROUTE_WEBSOCKET;
    new_route->method = strdup("GET");
    new_route->segments_count = split_path(path, new_route->path_segments);
    new_route->websocket_handler = handler;
    
    new_route->next_route = *route_list;
    *route_list = new_route;

    return BB_SUCCESS();
}

static int match_segments(bb_route_t *route, char segments[][MAX_PATH_LEN], int segments_count)
{
    if (segments_count != route->segments_count)
        return -1;
    for (int i = 0; i < segments_count; i++)
    {
        if (route->path_segments[i][0] == ':')
            continue;
        if (strcmp(route->path_segments[i], segments[i]) != 0)
            return -1;
    }
    return 0;
}

bb_route_t *bb_route_list_match(bb_route_list_t *route_list, bb_request_t *req)
{
    char req_segments[MAX_SEGMENTS][MAX_PATH_LEN];
    int req_count = split_path(bb_request_get_path(req), req_segments);

    for (bb_route_t *route = *route_list; route != NULL; route = route->next_route)
    {
        if (strcmp(bb_request_get_method(req), route->method) != 0) continue;

        if (match_segments(route, req_segments, req_count) == 0)
        {
            for (int j = 0; j < req_count; j++)
            {
                if (route->path_segments[j][0] == ':')
                    bb_request_add_param(req, route->path_segments[j] + 1, req_segments[j]);
            }
            return route;
        }
    }

    return NULL;
}

void bb_route_list_destroy(bb_route_list_t *route_list)
{
    bb_route_t *current = *route_list;
    while (current)
    {
        bb_route_t *next = current->next_route;
        free(current->method);
        free(current);
        current = next;
    }
    free(route_list);
}
