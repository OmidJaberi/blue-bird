#ifndef BB_SERVER_H
#define BB_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/runtime/runtime.h"
#include "blue-bird/error/error.h"

#include "http/handler.h"
#include "websocket/websocket.h"
#include "tls.h"
#include "blue-bird/web/metrics.h"

typedef struct bb_server bb_server_t;

bb_server_t *bb_server_create_on_runtime(bb_runtime_t *runtime, int port);
bb_server_t *bb_server_create_tls_on_runtime(bb_runtime_t *runtime, int port, const bb_tls_config_t *tls_config, bb_error_t *out_err);
void bb_server_add_route(bb_server_t *server, const char *method, const char *path, bb_http_handler_cb handler);
void bb_server_add_websocket(bb_server_t *server, const char *path, bb_ws_handler_cb handler);
void bb_server_set_websocket_heartbeat(bb_server_t *server, uint32_t interval_ms, uint32_t max_missed_pongs); // max_missed_pongs == 0 -> disable
void bb_server_use_pre_middleware(bb_server_t *server, bb_http_handler_cb mw);
void bb_server_use_post_middleware(bb_server_t *server, bb_http_handler_cb mw);
void bb_server_start(bb_server_t *server);

/*
 * Copy the server's current metrics into *out.
 * Returns 0 on success, -1 if server or out is NULL.
 *
 * Not thread-safe: call it from the runtime thread (e.g. inside a route
 * handler or runtime task), or while the runtime is stopped.
 */
int bb_server_get_metrics(const bb_server_t *server, bb_metrics_snapshot_t *out);

/*
 * Serve the metrics in Prometheus text format on GET <path> (default "/metrics"
 * when path is NULL). Opt-in: nothing is exposed until this is called.
 *
 * This registers an ordinary route, so it follows the router's rules: the first
 * route registered for a path wins (a user route added earlier on the same path
 * shadows it), and the server's pre/post middleware runs around it, so
 * authentication middleware protects it like any other route.
 *
 * Returns 0 on success. Returns -1 if server is NULL, path doesn't start with
 * '/', the route can't be registered, or metrics were already enabled (call it
 * once per server).
 */
int bb_server_enable_metrics(bb_server_t *server, const char *path);
void bb_server_destroy(bb_server_t *server);

static inline bb_server_t *bb_server_create(int port)
{
    return bb_server_create_on_runtime(bb_runtime_default(), port);
}

static inline bb_server_t *bb_server_create_tls(int port, const bb_tls_config_t *tls_config, bb_error_t *out_err)
{
    return bb_server_create_tls_on_runtime(bb_runtime_default(), port, tls_config, out_err);
}


#ifdef __cplusplus
}
#endif

#endif //BB_SERVER_H
