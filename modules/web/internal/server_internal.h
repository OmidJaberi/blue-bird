#ifndef BB_SERVER_INTERNAL_H
#define BB_SERVER_INTERNAL_H

#include "blue-bird/web/server.h"
#include "websocket/websocket_internal.h"
#include "router.h"
#include "middleware.h"

struct bb_server {
    bb_connection_t *connection;
    bb_runtime_t *runtime;
    bb_route_list_t *route_list;
    bb_middleware_list_t *pre_middleware_list; // Runs before the handler
    bb_middleware_list_t *post_middleware_list; // Runs after the handler
};

bb_error_t bb_server_run_request_pipeline(bb_server_t *server, bb_connection_t *conn, bb_websocket_t **ws, bb_request_t *req, bb_response_t *res);

#endif //BB_SERVER_INTERNAL_H
