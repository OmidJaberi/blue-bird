#ifndef BB_MIDDLEWARE_H
#define BB_MIDDLEWARE_H

#include "core/http.h"

typedef http_handler_cb middleware_cb;

typedef struct MiddlewareObject {
    middleware_cb middleware;
    struct MiddlewareObject *next;
} middleware_object_t;

typedef struct {
    middleware_object_t *first;
    int middleware_count;
} middleware_list_t;

void init_middleware_list(middleware_list_t *list);
middleware_object_t *create_middleware_object(middleware_cb mw);
void append_to_middleware_list(middleware_list_t *list, middleware_cb mw);
BBError run_middleware(middleware_list_t *list, request_t *req, response_t *res);
void destroy_middleware_list(middleware_list_t *list);

#endif //BB_MIDDLEWARE_H
