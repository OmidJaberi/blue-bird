#ifndef APP_MIDDLEWARE_H
#define APP_MIDDLEWARE_H

#include "core/http.h"

int logger_middleware(Request *req, Response *res);
int server_header_middleware(Request *req, Response *res);

#endif // APP_MIDDLEWARE_H
