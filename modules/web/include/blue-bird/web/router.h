#ifndef BB_ROUTER_H
#define BB_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "http.h"
#include "blue-bird/error/error.h"

typedef struct bb_route bb_route_t;
typedef bb_route_t* bb_route_list_t;

bb_http_handler_cb bb_route_get_handler(bb_route_t *route);
bb_route_list_t *bb_route_list_create(void);
bb_error_t bb_route_list_add(bb_route_list_t *route_list, const char *method, const char *path, bb_http_handler_cb handler);
bb_route_t *bb_route_list_match(bb_route_list_t *route_list, bb_request_t *req);
void bb_route_list_destroy(bb_route_list_t *route_list);


#ifdef __cplusplus
}
#endif

#endif //BB_ROUTER_H
