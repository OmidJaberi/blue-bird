#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include "core/http.h"

int hello_get_handler(Request *req, Response *res);
int hello_post_handler(Request *req, Response *res);
int root_handler(Request *req, Response *res);
int user_handler(Request *req, Response *res);
int comments_handler(Request *req, Response *res);

#endif // APP_HANDLERS_H
