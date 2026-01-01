#ifndef HTTP_H
#define HTTP_H

#include "http/request.h"
#include "http/response.h"
#include "error/error.h"

typedef BBError (*HttpHandler)(request_t *req, response_t *res);

#endif // HTTP_H
