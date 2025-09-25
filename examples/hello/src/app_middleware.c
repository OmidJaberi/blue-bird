#include "app_middleware.h"
#include <stdio.h>

int logger_middleware(Request *req, Response *res)
{
    printf("[Blue-Bird] %s %s\n", req->method, req->path);
    return 0;
}

int server_header_middleware(Request *req, Response *res)
{
    set_header(res, "Server", "Blue-Bird/0.1");
    return 0;
}
