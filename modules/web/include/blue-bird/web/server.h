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
/*
 * Persistent (keep-alive) connections. By default the server keeps an HTTP
 * connection open after a response so the client can send its next request on
 * it, and closes it after idle_timeout_ms of inactivity or after max_requests
 * requests.
 *
 *   idle_timeout_ms  how long a persistent connection may sit idle before the
 *                    server closes it. 0 disables keep-alive: every response
 *                    then carries "Connection: close" and the connection is
 *                    closed once it is written. Default 15000.
 *   max_requests     requests served on one connection before it is closed;
 *                    0 = no limit. Default 1000.
 *
 * The connection is closed regardless when the client asks for it ("Connection:
 * close", or an HTTP/1.0 request without "Connection: keep-alive"), when a
 * handler sets "Connection: close" on its response, and for WebSocket upgrades.
 * Pipelined requests are answered in order, each after the previous response
 * has been written.
 *
 * The idle timeout starts once the first response has been written; a
 * connection that has not completed a request yet is not timed out. Call this
 * before bb_server_start().
 */
void bb_server_set_keepalive(bb_server_t *server, uint32_t idle_timeout_ms, uint32_t max_requests);
void bb_server_use_pre_middleware(bb_server_t *server, bb_http_handler_cb mw);
void bb_server_use_post_middleware(bb_server_t *server, bb_http_handler_cb mw);
/*
 * Start accepting connections: registers the listening socket with the
 * server's runtime.
 *
 * Returns BB_SUCCESS() once the server is accepting, otherwise an error and
 * the server is left exactly as it was before the call (not started, still
 * safe to destroy, and bb_server_start() may be called again):
 *   BB_ERR_NULL      server is NULL
 *   BB_ERR_ALLOC     out of memory
 *   BB_ERR_INTERNAL  the server is already started, has no listening socket,
 *                    or the runtime refused to watch the listening socket
 */
bb_error_t bb_server_start(bb_server_t *server);

/*
 * Server lifecycle.
 *
 *   CREATED   bb_server_create*() returned; not accepting yet.
 *   RUNNING   bb_server_start() succeeded; accepting connections.
 *   DRAINING  bb_server_shutdown() was called: no longer accepting, finishing
 *             the work already in progress.
 *   STOPPED   shutdown has completed: no listening socket, no connections.
 *
 * CREATED -> RUNNING -> DRAINING -> STOPPED only ever moves forward, and a
 * server that has been shut down cannot be started again; create a new one.
 */
typedef enum
{
    BB_SERVER_STATE_CREATED,
    BB_SERVER_STATE_RUNNING,
    BB_SERVER_STATE_DRAINING,
    BB_SERVER_STATE_STOPPED
} bb_server_state_t;

/* STOPPED for a NULL server. */
bb_server_state_t bb_server_get_state(const bb_server_t *server);

/* Called once when a shutdown has completed (state is then STOPPED). */
typedef void (*bb_server_shutdown_cb)(bb_server_t *server, void *userdata);

/*
 * Gracefully shuts the server down.
 *
 * Immediately:
 *   - the listening socket is closed, so the port is released and new clients
 *     are refused;
 *   - connections that have not started a request (idle keep-alive ones
 *     included) are closed;
 *   - every WebSocket session is sent a close frame with code 1001 (going
 *     away) and closed.
 *
 * Requests already in progress are completed. Their responses carry
 * "Connection: close" and the connection closes once the response is written;
 * a WebSocket upgrade that arrives meanwhile is refused with 503.
 *
 * grace_ms bounds how long that may take. When it expires, whatever is still
 * open is closed without further ado. 0 means no grace at all.
 *
 * on_done (may be NULL) is called exactly once, from the runtime, when no
 * connection is left -- never from inside bb_server_shutdown() itself, and not
 * at all if the server is destroyed first. It may destroy the server and stop
 * the runtime, which is how a program normally ends:
 *
 *     static void done(bb_server_t *s, void *rt) { bb_runtime_stop(rt); }
 *     bb_server_shutdown(server, 5000, done, runtime);
 *     bb_runtime_run(runtime);            // returns once the server drained
 *     bb_server_destroy(server);
 *
 * Returns BB_SUCCESS() once the shutdown has begun. On error nothing has
 * changed and the server keeps running:
 *   BB_ERR_NULL      server is NULL
 *   BB_ERR_INTERNAL  already shutting down or stopped, or the runtime could not
 *                    schedule the shutdown
 *
 * Works from any state; a server that was never started simply has its
 * listening socket released.
 */
bb_error_t bb_server_shutdown(bb_server_t *server, uint32_t grace_ms, bb_server_shutdown_cb on_done, void *userdata);

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
/*
 * Immediate teardown: every connection is dropped as it is (responses in
 * flight are cut off, WebSocket sessions get no close frame). Valid in every
 * state; during a shutdown it cancels the shutdown, and its on_done callback is
 * not called. Use bb_server_shutdown() first for an orderly stop.
 */
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
