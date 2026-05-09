#ifndef APP_MIDDLEWARE_H
#define APP_MIDDLEWARE_H

#include <blue-bird/web/http.h>

BBError logger_middleware(request_t *req, response_t *res);
BBError server_header_middleware(request_t *req, response_t *res);

#endif // APP_MIDDLEWARE_H
