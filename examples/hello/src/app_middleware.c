#include "app_middleware.h"
#include <stdio.h>

BBError logger_middleware(request_t *req, response_t *res)
{
    printf("[Blue-Bird] %s %s\n", req->method, req->path);
    return BB_SUCCESS();
}

BBError server_header_middleware(request_t *req, response_t *res)
{
    set_header(res, "Server", "Blue-Bird/0.1");
    return BB_SUCCESS();
}
