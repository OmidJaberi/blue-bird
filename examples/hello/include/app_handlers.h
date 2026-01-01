#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include "core/http.h"

BBError hello_get_handler(request_t *req, response_t *res);
BBError hello_post_handler(request_t *req, response_t *res);
BBError root_handler(request_t *req, response_t *res);
BBError user_handler(request_t *req, response_t *res);
BBError comments_handler(request_t *req, response_t *res);

#endif // APP_HANDLERS_H
