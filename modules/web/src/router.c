#include "router.h"
#include "blue-bird/utils/platform.h"
#include "blue-bird/error/assert.h"
#include "blue-bird/web/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SEGMENTS 20
#define MAX_PATH_LEN 256


struct bb_route_list {
    bb_route_t *head;
    bb_route_t *tail;
};

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

static void append_route(bb_route_list_t *route_list, bb_route_t *new_route)
{
    new_route->next_route = NULL;

    if (route_list->tail)
    {
        route_list->tail->next_route = new_route;
    }
    else
    {
        route_list->head = new_route;
    }
    route_list->tail = new_route;
}

/*
 * Splits `path` into '/'-separated, non-empty segments.
 * Returns the segment count, or -1 if the path has more than MAX_SEGMENTS
 * segments or any segment is >= MAX_PATH_LEN bytes. Nothing is ever truncated,
 * so callers can never match a partial path.
 */
static int split_path(const char *path, char segments[MAX_SEGMENTS][MAX_PATH_LEN])
{
    int count = 0;
    const char *start = path;
    const char *p;

    for (p = path; ; p++)
    {
        if (*p != '/' && *p != '\0')
            continue;

        if (p > start)
        {
            size_t len = (size_t)(p - start);
            if (count >= MAX_SEGMENTS || len >= MAX_PATH_LEN)
                return -1;
            memcpy(segments[count], start, len);
            segments[count][len] = '\0';
            count++;
        }

        if (*p == '\0')
            break;
        start = p + 1;
    }

    return count;
}

/* Allocates a route and fills the fields shared by HTTP and WebSocket routes. */
static bb_error_t route_create(const char *method, const char *path, bb_route_t **out)
{
    bb_route_t *route = calloc(1, sizeof(*route));
    if (!route)
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate route");

    route->method = bb_strdup(method);
    if (!route->method)
    {
        free(route);
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate route method");
    }

    route->segments_count = split_path(path, route->path_segments);
    if (route->segments_count < 0)
    {
        free(route->method);
        free(route);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Route path has too many segments or a segment is too long");
    }

    *out = route;
    return BB_SUCCESS();
}

bb_error_t bb_route_list_add_http(bb_route_list_t *route_list, const char *method, const char *path, bb_http_handler_cb handler)
{
    // Basic sanity checks
    BB_ASSERT_MSG(route_list != NULL, "Route list pointer is NULL");
    BB_ASSERT_MSG(method != NULL, "HTTP method is NULL");
    BB_ASSERT_MSG(path != NULL, "Route path is NULL");
    BB_ASSERT_MSG(handler != NULL, "Route handler is NULL");

    bb_route_t *new_route = NULL;
    bb_error_t err = route_create(method, path, &new_route);
    if (err.code != BB_OK)
        return err;

    new_route->type = BB_ROUTE_HTTP;
    new_route->http_handler = handler;

    append_route(route_list, new_route);

    return BB_SUCCESS();
}

bb_error_t bb_route_list_add_websocket(bb_route_list_t *route_list, const char *path, bb_ws_handler_cb handler)
{
    // Basic sanity checks
    BB_ASSERT_MSG(route_list != NULL, "Route list pointer is NULL");
    BB_ASSERT_MSG(path != NULL, "Route path is NULL");
    BB_ASSERT_MSG(handler != NULL, "Route handler is NULL");

    bb_route_t *new_route = NULL;
    bb_error_t err = route_create("GET", path, &new_route);
    if (err.code != BB_OK)
        return err;

    new_route->type = BB_ROUTE_WEBSOCKET;
    new_route->websocket_handler = handler;

    append_route(route_list, new_route);

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
    if (req_count < 0)
        return NULL; // Oversized or too-deep path: cannot match any route (caller returns 404)

    for (bb_route_t *route = route_list->head; route != NULL; route = route->next_route)
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
    bb_route_t *current = route_list->head;
    while (current)
    {
        bb_route_t *next = current->next_route;
        free(current->method);
        free(current);
        current = next;
    }
    free(route_list);
}
