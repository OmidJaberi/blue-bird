#ifndef BB_CLIENT_INTERNAL_H
#define BB_CLIENT_INTERNAL_H

#include "blue-bird/runtime/runtime.h"

#include "blue-bird/web/http/handler.h"
#include "connection.h"

struct bb_client {
    bb_connection_t *connection;
    bb_runtime_t *runtime;
    bb_request_t *req;
    bb_response_t *res;
};

#endif //BB_CLIENT_INTERNAL_H
