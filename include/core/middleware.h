#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include "core/http.h"

#define MAX_MIDDLEWARE 20

typedef int (*Middleware)(Request *req, Response *res);

typedef struct MiddlewareObject {
    Middleware middleware;
    struct MiddlewareObject *next;
} MiddlewareObject;

typedef struct {
    MiddlewareObject *first;
    int middleware_count;
} MiddlewareList;

void init_middleware_list(MiddlewareList *list);
MiddlewareObject *create_middleware_object(Middleware mw);
void append_to_middleware_list(MiddlewareList *list, Middleware mw);
int run_middleware(MiddlewareList *list, Request *req, Response *res);
void destroy_middleware_list(MiddlewareList *list);

#endif // MIDDLEWARE_H
