#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include "core/http.h"

#define MAX_MIDDLEWARE 20

typedef int (*Middleware)(Request *req, Response *res);

void use_middleware(Middleware mw);

int run_middleware(Request *req, Response *res);

// Just for unit testing, temporaty:
void clear_middleware();

#endif // MIDDLEWARE_H
