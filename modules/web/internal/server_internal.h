#ifndef BB_SERVER_INTERNAL_H
#define BB_SERVER_INTERNAL_H

#include "blue-bird/web/server.h"
#include "websocket/websocket_internal.h"
#include "connection/async_connection.h"
#include "connection/conn_list.h"
#include "router.h"
#include "middleware.h"

typedef struct {
    bb_server_t *server;
    bb_async_connection_t *async_conn;
    bb_websocket_t *ws;

    bb_conn_node_t *conn_node;
} bb_server_task_data_t;

struct bb_server {
    bb_async_connection_t *async_conn;
    bb_runtime_t *runtime;
    bb_task_t *accept_task;
    bb_server_task_data_t *accept_task_data;
    bb_route_list_t *route_list;
    bb_middleware_list_t *pre_middleware_list; // Runs before the handler
    bb_middleware_list_t *post_middleware_list; // Runs after the handler

    bb_conn_list_t *conn_list;
};

#endif //BB_SERVER_INTERNAL_H
