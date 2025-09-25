#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include "core/http.h"

void hello_get_handler(Request *req, Response *res);
void hello_post_handler(Request *req, Response *res);
void root_handler(Request *req, Response *res);
void user_handler(Request *req, Response *res);
void comments_handler(Request *req, Response *res);

#endif // APP_HANDLERS_H
