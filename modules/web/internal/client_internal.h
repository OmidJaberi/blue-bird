#ifndef BB_CLIENT_INTERNAL_H
#define BB_CLIENT_INTERNAL_H

#include "blue-bird/runtime/runtime.h"

#include "blue-bird/web/http/handler.h"
#include "connection/async_connection.h"
#include "http/http_parser.h"

struct bb_client {
    bb_async_connection_t *async_conn;
    bb_connection_t *connection;
    bb_runtime_t *runtime;
    bb_request_t *req;
    bb_response_t *res;
    bb_http_parser_t *http_parser;
    size_t parsed_offset;
};

#endif //BB_CLIENT_INTERNAL_H
