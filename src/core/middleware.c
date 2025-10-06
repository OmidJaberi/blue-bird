#include "core/middleware.h"
#include <stdio.h>

void init_middleware_list(MiddlewareList *list)
{
    list->middleware_count = 0;
}

void append_to_middleware_list(MiddlewareList *list, Middleware mw)
{
    if (list->middleware_count >= MAX_MIDDLEWARE)
    {
        fprintf(stderr, "Max middleware reached!\n");
        return;
    }
    list->middleware_list[list->middleware_count++] = mw;
}

int run_middleware(MiddlewareList *list, Request *req, Response *res)
{
    for (int i = 0; i < list->middleware_count; i++)
    {
        int result = list->middleware_list[i](req, res);
        if (result != 0)
            return result;
    }
    return 0;
}
