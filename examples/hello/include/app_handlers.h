#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include <blue-bird/web/http.h>

bb_error_t root_handler(bb_request_t *req, bb_response_t *res);
bb_error_t hello_post_handler(bb_request_t *req, bb_response_t *res);
bb_error_t hello_get_handler(bb_request_t *req, bb_response_t *res);

#endif // APP_HANDLERS_H
