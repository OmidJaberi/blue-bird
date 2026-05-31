#include "blue-bird/web/middleware.h"
#include <stdio.h>
#include <stdlib.h>

void bb_middleware_list_init(bb_middleware_list_t *list)
{
    *list = NULL;
}

static bb_middleware_object_t *create_middleware_object(bb_http_handler_cb mw)
{
    bb_middleware_object_t *mw_obj = malloc(sizeof(bb_middleware_object_t));
    if (mw_obj == NULL)
    {
        return NULL;
    }
    mw_obj->middleware = mw;
    mw_obj->next = NULL;
    return mw_obj;
}

void bb_middleware_list_append(bb_middleware_list_t *list, bb_http_handler_cb mw)
{
    bb_middleware_object_t *mw_obj = create_middleware_object(mw);
    if (!*list)
        *list = mw_obj;
    else
    {
        bb_middleware_object_t *last = *list;
        while (last->next)
            last = last->next;
        last->next = mw_obj;
    }
}

bb_error_t bb_middleware_list_run(bb_middleware_list_t *list, bb_request_t *req, bb_response_t *res)
{
    bb_middleware_object_t *current = *list;
    while (current)
    {
        bb_error_t result = current->middleware(req, res);
        if (BB_FAILED(result))
            return result;
        current = current->next;
    }
    return BB_SUCCESS();
}

void bb_middleware_list_destroy(bb_middleware_list_t *list)
{
    bb_middleware_object_t *current = *list;
    while (current)
    {
        bb_middleware_object_t *next = current->next;
        free(current);
        current = next;
    }
}
