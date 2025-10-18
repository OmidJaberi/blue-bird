#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include "core/http.h"

BBError hello_get_handler(Request *req, Response *res);
BBError hello_post_handler(Request *req, Response *res);
BBError root_handler(Request *req, Response *res);
BBError user_handler(Request *req, Response *res);
BBError comments_handler(Request *req, Response *res);

#endif // APP_HANDLERS_H
