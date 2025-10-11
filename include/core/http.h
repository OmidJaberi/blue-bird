#ifndef HTTP_H
#define HTTP_H

#include "http/request.h"
#include "http/response.h"

typedef int (*HttpHandler)(Request *req, Response *res);

#endif // HTTP_H
