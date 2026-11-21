#ifndef BB_SERVER_INTERNAL_H
#define BB_SERVER_INTERNAL_H

#include "blue-bird/web/server.h"
#include "websocket/websocket_internal.h"
#include "websocket/websocket_list.h"
#include "connection/async_connection.h"
#include "http/parser.h"
#include "connection/conn_list.h"
#include "router.h"
#include "middleware.h"
#include "metrics.h"

/* Persistent-connection defaults; see bb_server_set_keepalive(). */
#define BB_SERVER_DEFAULT_KEEPALIVE_IDLE_MS      15000
#define BB_SERVER_DEFAULT_KEEPALIVE_MAX_REQUESTS 1000

typedef struct {
    bb_server_t *server;
    bb_async_connection_t *async_conn;
    bb_websocket_t *ws;

    bb_conn_node_t *conn_node;
    bb_http_parser_t *http_parser;
    size_t parsed_offset;
    int64_t request_start_ms; // monotonic time of the first read step; -1 until then

    bool keep_alive;           // the response being written leaves the connection open for another request
    uint32_t requests_served;  // responses already written on this connection
    int64_t last_activity_ms;  // monotonic: last read activity, or when the connection went idle
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
    bb_ws_list_t *ws_list;

    bb_metrics_t metrics; // zero-initialised by calloc in server creation
    int metrics_enabled;  // set by bb_server_enable_metrics(); the route itself lives in route_list

    uint32_t ws_heartbeat_interval_ms;
    uint32_t ws_heartbeat_max_missed_pongs;

    uint32_t keepalive_idle_timeout_ms; // 0 = keep-alive disabled
    uint32_t keepalive_max_requests;    // per connection; 0 = unlimited
    bb_task_t *keepalive_sweep_task;    // closes idle persistent connections; created with the first one

    bb_tls_context_t *tls_ctx; // NULL for a plain-TCP server
};

#endif //BB_SERVER_INTERNAL_H
