#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include "core/http.h"

#define MAX_MIDDLEWARE 20

typedef int (*Middleware)(Request *req, Response *res);

typedef struct {
    Middleware middleware_list[MAX_MIDDLEWARE];
    int middleware_count;
} MiddlewareList;

void init_middleware_list(MiddlewareList *list);
void append_to_middleware_list(MiddlewareList *list, Middleware mw);
int run_middleware(MiddlewareList *list, Request *req, Response *res);

#endif // MIDDLEWARE_H
