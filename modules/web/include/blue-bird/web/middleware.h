#ifndef BB_MIDDLEWARE_H
#define BB_MIDDLEWARE_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/http.h"

typedef struct BBMiddlewareObject {
    bb_http_handler_cb middleware;
    struct BBMiddlewareObject *next;
} bb_middleware_object_t;

typedef bb_middleware_object_t* bb_middleware_list_t;

void bb_middleware_list_init(bb_middleware_list_t *list);
void bb_middleware_list_append(bb_middleware_list_t *list, bb_http_handler_cb mw);
bb_error_t bb_middleware_list_run(bb_middleware_list_t *list, bb_request_t *req, bb_response_t *res);
void bb_middleware_list_destroy(bb_middleware_list_t *list);


#ifdef __cplusplus
}
#endif

#endif //BB_MIDDLEWARE_H
