#ifndef HTTP_H
#define HTTP_H

#include "http/request.h"
#include "http/response.h"
#include "error/error.h"

typedef BBError (*HttpHandler)(Request *req, Response *res);

#endif // HTTP_H
