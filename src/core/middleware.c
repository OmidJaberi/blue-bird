#include "core/middleware.h"
#include <stdio.h>
#include <stdlib.h>

void init_middleware_list(MiddlewareList *list)
{
    list->first = NULL;
    list->middleware_count = 0;
}

MiddlewareObject *create_middleware_object(Middleware mw)
{
    MiddlewareObject *mw_obj = malloc(sizeof(MiddlewareObject));
    if (mw_obj == NULL)
    {
        return NULL;
    }
    mw_obj->middleware = mw;
    mw_obj->next = NULL;
    return mw_obj;
}

void append_to_middleware_list(MiddlewareList *list, Middleware mw)
{
    MiddlewareObject *mw_obj = create_middleware_object(mw);
    if (!list->first)
        list->first = mw_obj;
    else
    {
        MiddlewareObject *last = list->first;
        while (last->next)
            last = last->next;
        last->next = mw_obj;
    }
}

BBError run_middleware(MiddlewareList *list, Request *req, Response *res)
{
    MiddlewareObject *current = list->first;
    while (current)
    {
        BBError result = current->middleware(req, res);
        if (BB_FAILED(result))
            return result;
        current = current->next;
    }
    return BB_SUCCESS();
}

void destroy_middleware_list(MiddlewareList *list)
{
    MiddlewareObject *current = list->first;
    while (current)
    {
        MiddlewareObject *next = current->next;
        free(current);
        current = next;
    }
}
