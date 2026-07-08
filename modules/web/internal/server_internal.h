#ifndef BB_SERVER_INTERNAL_H
#define BB_SERVER_INTERNAL_H

#include "blue-bird/web/server.h"
#include "websocket/websocket_internal.h"
#include "connection/async_tasks.h"
#include "router.h"
#include "middleware.h"

struct bb_server {
    bb_async_connection_t *async_conn;
    bb_runtime_t *runtime;
    bb_route_list_t *route_list;
    bb_middleware_list_t *pre_middleware_list; // Runs before the handler
    bb_middleware_list_t *post_middleware_list; // Runs after the handler
};

#endif //BB_SERVER_INTERNAL_H
