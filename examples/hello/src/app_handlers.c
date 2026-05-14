#include "app_handlers.h"

#include <stdio.h>
#include <string.h>

bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Blue-Bird :)");
    return BB_SUCCESS();
}

bb_error_t hello_post_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello via POST!");
    return BB_SUCCESS();
}

bb_error_t hello_get_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    const char *name = bb_request_get_query_param(req, "name");
    if (name)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "Hello %s, via GET!", name);
        bb_response_set_body(res, buf);
    }
    else
        bb_response_set_body(res, "Hello via GET!");
    return BB_SUCCESS();
}
