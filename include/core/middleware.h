#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include "core/http.h"

typedef http_handler_cb middleware_cb;

typedef struct MiddlewareObject {
    middleware_cb middleware;
    struct MiddlewareObject *next;
} MiddlewareObject;

typedef struct {
    MiddlewareObject *first;
    int middleware_count;
} MiddlewareList;

void init_middleware_list(MiddlewareList *list);
MiddlewareObject *create_middleware_object(middleware_cb mw);
void append_to_middleware_list(MiddlewareList *list, middleware_cb mw);
BBError run_middleware(MiddlewareList *list, request_t *req, response_t *res);
void destroy_middleware_list(MiddlewareList *list);

#endif // MIDDLEWARE_H
