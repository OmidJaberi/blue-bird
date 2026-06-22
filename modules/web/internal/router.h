#ifndef BB_ROUTER_H
#define BB_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/http/handler.h"
#include "blue-bird/web/websocket/websocket.h"
#include "blue-bird/error/error.h"

typedef enum {
    BB_ROUTE_HTTP,
    BB_ROUTE_WEBSOCKET
} bb_route_type_t;

typedef struct bb_route bb_route_t;
typedef bb_route_t* bb_route_list_t;

bb_route_type_t bb_route_get_type(bb_route_t *route);
bb_http_handler_cb bb_route_get_http_handler(bb_route_t *route);
bb_ws_handler_cb bb_route_get_websocket_handler(bb_route_t *route);

bb_route_list_t *bb_route_list_create(void);
bb_error_t bb_route_list_add_http(bb_route_list_t *route_list, const char *method, const char *path, bb_http_handler_cb handler);
bb_error_t bb_route_list_add_websocket(bb_route_list_t *route_list, const char *path, bb_ws_handler_cb handler);
bb_route_t *bb_route_list_match(bb_route_list_t *route_list, bb_request_t *req);
void bb_route_list_destroy(bb_route_list_t *route_list);



#ifdef __cplusplus
}
#endif

#endif //BB_ROUTER_H
