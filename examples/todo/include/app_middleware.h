#ifndef APP_MIDDLEWARE_H
#define APP_MIDDLEWARE_H

#include <blue-bird/web/http.h>

bb_error_t logger_middleware(bb_request_t *req, bb_response_t *res);
bb_error_t server_header_middleware(bb_request_t *req, bb_response_t *res);

#endif // APP_MIDDLEWARE_H
