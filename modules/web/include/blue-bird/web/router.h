#ifndef BB_ROUTER_H
#define BB_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "http.h"
#include "executor.h"
#include "blue-bird/error/error.h"

#define MAX_SEGMENTS 20
#define MAX_PATH_LEN 256

typedef bb_http_handler_cb bb_route_handler_cb;

typedef struct Route {
    char *method;
    char path_segments[MAX_SEGMENTS][MAX_PATH_LEN];
    int segments_count;
    bb_route_handler_cb handler;
    struct Route *next_route;
} bb_route_t;

typedef bb_route_t* bb_route_list_t;

void bb_route_list_init(bb_route_list_t *route_list);
bb_error_t bb_route_list_add(bb_route_list_t *route_list, const char *method, const char *path, bb_route_handler_cb handler);
void bb_route_list_handle_request(bb_route_list_t *route_list, bb_request_t *req, bb_response_t *res, bb_web_executor_t *executor);
void bb_route_list_destroy(bb_route_list_t *route_list);


#ifdef __cplusplus
}
#endif

#endif //BB_ROUTER_H
