#include "server_internal.h"
#include "connection/async_connection.h"
#include "http/parser.h"
#include "http/request.h"

#include "blue-bird/runtime/event.h"
#include "blue-bird/log/log.h"
#include "blue-bird/utils/time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static bb_error_t default_404(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 404);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Route Not Found");
    return BB_SUCCESS();
}

static bb_error_t default_400(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 400);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Bad Request");
    return BB_SUCCESS();
}

/*
 * Takes ownership of `tls_ctx` (may be NULL for a plain server):
 * on success it's stored on the returned server; on any failure it is
 * destroyed here before returning NULL. Callers must not touch it again
 * either way.
 */
static bb_server_t *_bb_server_create_common(bb_runtime_t *runtime, int port, bb_tls_context_t *tls_ctx)
{
    bb_server_t *server = NULL;

    if (!runtime)
    {
        goto fail;
    }

    server = calloc(1, sizeof(*server));
    if (!server)
    {
        goto fail;
    }

    // Ownership of tls_ctx transfers to the server only once it is
    // assigned; before that point the fail path destroys it directly.
    server->runtime = runtime;
    server->tls_ctx = tls_ctx;
    server->keepalive_idle_timeout_ms = BB_SERVER_DEFAULT_KEEPALIVE_IDLE_MS;
    server->keepalive_max_requests = BB_SERVER_DEFAULT_KEEPALIVE_MAX_REQUESTS;

    // Cheap, purely in-memory resources first.
    server->route_list = bb_route_list_create();
    if (!server->route_list)
    {
        goto fail;
    }

    server->pre_middleware_list = bb_middleware_list_create();
    if (!server->pre_middleware_list)
    {
        goto fail;
    }

    server->post_middleware_list = bb_middleware_list_create();
    if (!server->post_middleware_list)
    {
        goto fail;
    }

    server->conn_list = bb_conn_list_create();
    if (!server->conn_list)
    {
        goto fail;
    }

    server->ws_list = bb_ws_list_create();
    if (!server->ws_list)
    {
        goto fail;
    }

    server->accept_task = NULL;
    server->accept_task_data = NULL;

    // The listening socket itself is always plain TCP -- TLS is applied
    // per accepted client connection, not to the accept() socket.
    // Acquired last: it's the only externally visible side effect
    // (binds the port), so no earlier failure ever has to undo it.
    server->async_conn = bb_async_connection_serve(runtime, port);
    if (!server->async_conn)
    {
        goto fail;
    }

    BB_LOG_INFO("Blue-Bird %sserver initialized on port %d\n", tls_ctx ? "TLS " : "", port);
    return server;

fail:
    if (server)
        bb_server_destroy(server);   // must destroy tls_ctx too, and tolerate NULL members
    else
        bb_tls_context_destroy(tls_ctx);
    return NULL;
}

bb_server_t *bb_server_create_on_runtime(bb_runtime_t *runtime, int port)
{
    return _bb_server_create_common(runtime, port, NULL);
}

bb_server_t *bb_server_create_tls_on_runtime(bb_runtime_t *runtime, int port, const bb_tls_config_t *tls_config, bb_error_t *out_err)
{
    if (out_err)
    {
        *out_err = BB_SUCCESS();
    }

    if (!runtime)
    {
        if (out_err) *out_err = BB_ERROR(BB_ERR_NULL, "No runtime.");
        return NULL;
    }

    // Fail fast: never defer certificate/key problems until the first
    // client connects.
    bb_error_t tls_err;
    bb_tls_context_t *tls_ctx = bb_tls_context_create_server(tls_config, &tls_err);
    if (!tls_ctx)
    {
        if (out_err) *out_err = tls_err;
        return NULL;
    }

    // On failure, _bb_server_create_common() has already torn down the
    // (partially-constructed) server via bb_server_destroy(), which owns
    // tls_ctx from this point on and frees it -- don't double-free it here.
    bb_server_t *server = _bb_server_create_common(runtime, port, tls_ctx);
    if (!server)
    {
        if (out_err) *out_err = BB_ERROR(BB_ERR_INTERNAL, "Failed to create server.");
        return NULL;
    }

    return server;
}

static void _server_ws_closed(bb_websocket_t *ws, void *userdata)
{
    bb_server_t *server = userdata;

    if (!server || !server->ws_list)
    {
        return;
    }

    bb_ws_list_remove(server->ws_list, ws);
    bb_websocket_destroy(ws);
}

static void _server_on_disconnect(void *userdata);

/*
 * The async connection invokes its disconnect callback when the peer closes
 * before a response is written (EOF while reading). Without a hook here nothing
 * ever released that connection: its fd, conn_list node and task data leaked.
 * The hook is only installed during the read phase and is removed before any
 * other teardown or write path, so it can never run a second cleanup.
 * WebSocket upgrades replace it with the WebSocket's own callback, which is
 * left alone.
 */
static void _server_clear_disconnect_hook(bb_async_connection_t *async_conn)
{
    if (async_conn && async_conn->disconnect == _server_on_disconnect)
    {
        bb_async_connection_set_disconnect_callback(async_conn, NULL, NULL);
    }
}

static void _server_task_data_cleanup(bb_server_task_data_t *data)
{
    if (!data)
    {
        return;
    }

    _server_clear_disconnect_hook(data->async_conn);

    if (data->server && data->server->conn_list)
    {
        bb_conn_list_remove(data->server->conn_list, data->conn_node);
    }

    if (data->http_parser)
    {
        bb_http_parser_destroy(data->http_parser);
        data->http_parser = NULL;
    }

    if (data->ws)
    {
        bb_websocket_destroy(data->ws);
    }
    else if (data->async_conn)
    {
        bb_async_connection_destroy(data->async_conn);
    }

    free(data);
}

static void _server_on_disconnect(void *userdata)
{
    _server_task_data_cleanup((bb_server_task_data_t *) userdata);
}

static bb_read_status_t _server_read_step(void *userdata);
static void _server_read_error(bb_error_t err, void *userdata);

/* ------------------------------------------------------------------------ */
/* Persistent connections                                                    */
/* ------------------------------------------------------------------------ */

/* True if the comma-separated header value contains `token` (case-insensitive). */
static bool _header_value_has_token(const char *value, const char *token)
{
    size_t token_len = strlen(token);
    const char *p = value;

    while (p && *p)
    {
        while (*p == ' ' || *p == '\t' || *p == ',')
        {
            p++;
        }

        const char *start = p;
        while (*p && *p != ',')
        {
            p++;
        }

        const char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
        {
            end--;
        }

        if ((size_t)(end - start) == token_len && bb_strncasecmp(start, token, token_len) == 0)
        {
            return true;
        }
    }

    return false;
}

/* RFC 9112 section 9.3: HTTP/1.1 connections are persistent unless the client says
 * "close"; older ones only when the client asks for "keep-alive". */
static bool _request_wants_keep_alive(const bb_http_request_t *req)
{
    bool close_requested = false;
    bool keep_alive_requested = false;

    for (size_t i = 0; i < req->header_count; i++)
    {
        if (bb_strcasecmp(req->headers[i].name, "Connection") != 0)
        {
            continue;
        }
        if (_header_value_has_token(req->headers[i].value, "close"))
        {
            close_requested = true;
        }
        if (_header_value_has_token(req->headers[i].value, "keep-alive"))
        {
            keep_alive_requested = true;
        }
    }

    if (close_requested)
    {
        return false;
    }

    if (req->version_major > 1 || (req->version_major == 1 && req->version_minor >= 1))
    {
        return true;
    }

    return keep_alive_requested;
}

static void _server_keepalive_sweep(bb_task_t *task, void *userdata);

/* Idle persistent connections are reclaimed by a periodic sweep. It is created
 * lazily, when the first connection wants to stay open; if it cannot be, that
 * connection is simply not kept alive, so a persistent connection can never
 * outlive the thing that would close it. */
static bool _server_ensure_keepalive_sweep(bb_server_t *server)
{
    if (server->keepalive_sweep_task)
    {
        return true;
    }

    uint64_t interval_ms = server->keepalive_idle_timeout_ms / 4;
    if (interval_ms < 10)
    {
        interval_ms = 10;
    }
    if (interval_ms > 1000)
    {
        interval_ms = 1000;
    }

    server->keepalive_sweep_task = bb_runtime_set_interval(server->runtime, interval_ms, _server_keepalive_sweep, server);
    return server->keepalive_sweep_task != NULL;
}

/* Whether the connection stays open after the response being built for `raw`. */
static bool _server_should_keep_alive(bb_server_task_data_t *data, const bb_http_request_t *raw, bb_response_t *res)
{
    bb_server_t *server = data->server;

    if (server->keepalive_idle_timeout_ms == 0)
    {
        return false;
    }

    if (!_request_wants_keep_alive(raw))
    {
        return false;
    }

    const char *connection_header = bb_response_get_header(res, "Connection");
    if (connection_header && _header_value_has_token(connection_header, "close"))
    {
        return false;
    }

    if (server->keepalive_max_requests != 0 && data->requests_served + 1 >= server->keepalive_max_requests)
    {
        return false;
    }

    return _server_ensure_keepalive_sweep(server);
}

/*
 * The response has been written and the connection stays open: get it ready
 * for the next request, or close it if that cannot be done. Consumes `data`
 * either way (it is freed when the connection is closed).
 */
static void _server_next_request(bb_server_task_data_t *data)
{
    bb_async_connection_t *async_conn = data->async_conn;
    bb_connection_t *conn = async_conn ? async_conn->connection : NULL;

    if (!conn)
    {
        _server_task_data_cleanup(data);
        return;
    }

    data->requests_served++;
    data->keep_alive = false;
    data->request_start_ms = -1;
    data->last_activity_ms = bb_time_monotonic_ms();

    /* Forget the request just answered, but keep what follows it: with
     * pipelining that is the start of the next request. */
    if (data->http_parser)
    {
        bb_http_parser_reset(data->http_parser);
    }
    bb_connection_discard_read(conn, data->parsed_offset);
    data->parsed_offset = 0;
    conn->state = BB_CONNECTION_READING;

    /* The hook was removed when the write started; while the connection waits
     * for its next request a peer that hangs up must release it again. */
    bb_async_connection_set_disconnect_callback(async_conn, _server_on_disconnect, data);

    /* Reading was paused while the response went out. */
    if (BB_FAILED(bb_async_connection_create_read_task(async_conn, _server_read_step, _server_read_error, data)))
    {
        _server_task_data_cleanup(data);
        return;
    }

    /* A pipelined request may already be buffered. No new bytes will arrive
     * to wake the read task for it, so run it now. */
    if (conn->buffer_length > 0)
    {
        bb_read_status_t status = _server_read_step(data);
        if (status.result == BB_READ_ERROR)
        {
            _server_read_error(status.err, data);
        }
    }
}

typedef struct
{
    int64_t now_ms;
    int64_t idle_timeout_ms;
} _server_sweep_ctx_t;

static void _server_sweep_visit(void *item, void *ctx_ptr)
{
    bb_server_task_data_t *data = item;
    const _server_sweep_ctx_t *ctx = ctx_ptr;

    /* The timeout only applies to a connection that has already completed a
     * request, and never while a response is going out. */
    if (data->requests_served == 0)
    {
        return;
    }

    if (!data->async_conn || !data->async_conn->connection || data->async_conn->connection->state == BB_CONNECTION_WRITING)
    {
        return;
    }

    if (ctx->now_ms - data->last_activity_ms < ctx->idle_timeout_ms)
    {
        return;
    }

    _server_task_data_cleanup(data);
}

static void _server_keepalive_sweep(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_t *server = userdata;

    if (!server->conn_list || server->keepalive_idle_timeout_ms == 0)
    {
        return;
    }

    _server_sweep_ctx_t ctx = { bb_time_monotonic_ms(), server->keepalive_idle_timeout_ms };
    bb_conn_list_foreach(server->conn_list, _server_sweep_visit, &ctx);
}

static void _server_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_server_task_data_t *data = userdata;
    bb_server_t *server = data->server;
    if (data->ws)
    {
        if (data->http_parser)
        {
            bb_http_parser_destroy(data->http_parser);
            data->http_parser = NULL;
        }

        if (bb_ws_list_add(server->ws_list, data->ws) != 0)
        {
            bb_websocket_destroy(data->ws);
        }
        else
        {
            bb_metrics_on_ws_session_opened(&server->metrics);

            /*
            * Now websocket session owns the connection.
            */
            data->async_conn = NULL;
            bb_websocket_set_close_callback(data->ws, _server_ws_closed, server);

            if (server->ws_heartbeat_max_missed_pongs > 0)
            {
                bb_websocket_set_heartbeat(data->ws, server->ws_heartbeat_interval_ms, server->ws_heartbeat_max_missed_pongs);
            }

            bb_error_t err = bb_websocket_create_read_task(data->ws);
            if (BB_FAILED(err))
            {
                _server_ws_closed(data->ws, server);
            }
        }

        if (data->server && data->server->conn_list)
        {
            bb_conn_list_remove(data->server->conn_list, data->conn_node);
        }
        free(data);
        return;
    }

    if (data->keep_alive)
    {
        _server_next_request(data);
        return;
    }

    _server_task_data_cleanup(data);
}

static void _server_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    _server_task_data_cleanup((bb_server_task_data_t *) userdata);
}

static int _server_create_write_task(bb_server_task_data_t *data)
{
    /* From here the write task owns the connection's fate (write_failure cleans up). */
    _server_clear_disconnect_hook(data->async_conn);

    if (BB_FAILED(bb_async_connection_create_write_task(data->async_conn, _server_after_write, _server_write_error, data)))
        return -1;
    return 0;
}

static void _server_read_error(bb_error_t err, void *userdata)
{
    bb_server_task_data_t *data = userdata;

    if (err.code != BB_ERR_EOF)
    {
        BB_LOG_ERROR("%s: %s\n", bb_strerror(err.code), err.msg);
    }

    _server_task_data_cleanup(data);
}

/* A middleware or handler that returns an error stops the pipeline, but the
 * client must still get an answer; closing the connection without one leaves
 * it hanging or reporting an empty reply.
 *
 * If the failing step already chose an error status (>= 400) that response is
 * sent as it stands: that is how a handler says "404" or an auth middleware
 * says "401". Otherwise the status and body are replaced by a plain 500 (any
 * headers already set stay). The error itself goes to the log, never to the
 * client. */
static bb_error_t _http_pipeline_failed(bb_error_t err, bb_response_t *res)
{
    int status = bb_response_get_status(res);

    if (status < 400)
    {
        bb_response_set_status(res, 500);
        bb_response_set_header(res, "Content-Type", "text/plain");
        bb_response_set_body(res, "Internal Server Error");
        status = 500;
    }

    /* A deliberate 4xx is an ordinary answer, not worth a log line. */
    if (status >= 500)
    {
        BB_LOG_ERROR("%s: %s\n", bb_strerror(err.code), err.msg);
    }

    return BB_SUCCESS();
}

static bb_error_t _run_http_route(bb_server_t *server, bb_route_t *route, bb_request_t *req, bb_response_t *res)
{
    bb_error_t err;

    err = bb_middleware_list_run(server->pre_middleware_list, req, res);

    if (BB_FAILED(err))
    {
        return _http_pipeline_failed(err, res);
    }

    err = bb_route_run_http_handler(route, req, res);

    if (BB_FAILED(err))
    {
        return _http_pipeline_failed(err, res);
    }

    err = bb_middleware_list_run(server->post_middleware_list, req, res);

    if (BB_FAILED(err))
    {
        return _http_pipeline_failed(err, res);
    }

    return err;
}

static bb_error_t _run_websocket_route(bb_async_connection_t *async_conn, bb_route_t *route, bb_websocket_t **ws, bb_request_t *req, bb_response_t *res)
{
    *ws = bb_websocket_accept(async_conn, req, res, bb_route_get_websocket_handler(route));

    if (!(*ws))
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to create websocket.");
    }

    return BB_SUCCESS();
}

/* Handler for the opt-in metrics route; ctx is the owning bb_server_t. */
static bb_error_t _metrics_route_handler(void *ctx, bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_server_t *server = ctx;

    bb_metrics_snapshot_t snap;
    if (bb_server_get_metrics(server, &snap) != 0)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to read metrics.");
    }

    size_t len = bb_metrics_render_prometheus(&snap, NULL, 0);
    char *body = malloc(len + 1);
    if (!body)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate metrics body.");
    }
    bb_metrics_render_prometheus(&snap, body, len + 1);

    bb_response_set_status(res, 200);
    bb_response_set_header(res, "Content-Type", "text/plain; version=0.0.4; charset=utf-8");
    bb_response_set_body(res, body);
    free(body);

    return BB_SUCCESS();
}

bb_error_t _run_request_pipeline(bb_server_t *server, bb_async_connection_t *async_conn, bb_websocket_t **ws, bb_request_t *req, bb_response_t *res)
{
    bb_route_t *route = bb_route_list_match(server->route_list, req);
    if (!route)
    {
        return default_404(req, res);
    }

    switch (bb_route_get_type(route))
    {
        case BB_ROUTE_HTTP:
            return _run_http_route(server, route, req, res);
        case BB_ROUTE_WEBSOCKET:
            return _run_websocket_route(async_conn, route, ws, req, res);
        default:
            return BB_ERROR(BB_ERR_INTERNAL, "Unknown route type");
    }
}

static bb_read_status_t _server_read_step(void *userdata)
{
    bb_server_task_data_t *data = userdata;
    bb_async_connection_t *async_conn = data->async_conn;
    bb_connection_t *conn = async_conn->connection;

    int64_t now_ms = bb_time_monotonic_ms();
    data->last_activity_ms = now_ms;
    if (data->request_start_ms < 0)
    {
        data->request_start_ms = now_ms;
    }

    if (!data->http_parser)
    {
        data->http_parser = bb_http_parser_create();
        if (!data->http_parser)
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_ALLOC, "Failed to create HTTP parser.") };
    }

    if (data->parsed_offset > conn->buffer_length)
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "HTTP parser buffer offset is invalid.") };

    size_t available = conn->buffer_length - data->parsed_offset;
    bb_http_parse_status_t parse_status = bb_http_parser_feed(
        data->http_parser,
        (const uint8_t *)conn->buffer + data->parsed_offset,
        available);
    data->parsed_offset += bb_http_parser_last_consumed(data->http_parser);

    if (parse_status == BB_HTTP_PARSE_INCOMPLETE)
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };

    if (parse_status == BB_HTTP_PARSE_ERROR)
    {
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_BAD_REQUEST, bb_http_parser_error(data->http_parser)) };
    }

    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    if (!req || !res)
    {
        if (req) bb_request_destroy(req);
        if (res) bb_response_destroy(res);
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_ALLOC, "Failed to allocate HTTP request/response.") };
    }

    if (bb_request_parse_http_parser(bb_http_parser_get_request(data->http_parser), req))
    {
        default_400(req, res);
    }
    else
    {
        bb_error_t err = _run_request_pipeline(data->server, async_conn, &data->ws, req, res);

        if (BB_FAILED(err))
        {
            bb_request_destroy(req);
            bb_response_destroy(res);

            return (bb_read_status_t){ BB_READ_ERROR, err };
        }
    }

    const bb_http_request_t *raw = bb_http_parser_get_request(data->http_parser);
    int status = bb_response_get_status(res);

    /* On a persistent connection anything sent beyond the response's real
     * framing would be read by the client as the start of the next response.
     * 1xx/204/304 have no body at all; a HEAD response keeps the headers (and
     * the Content-Length the body would have had) but not the body itself. */
    if (status < 200 || status == 204 || status == 304)
    {
        bb_response_set_body(res, NULL);
    }

    /* The 101 upgrade response leaves the connection open for the WebSocket
     * and says so itself. Every other response states what happens next. */
    bool keep_alive = false;
    if (status != 101)
    {
        keep_alive = !data->ws && _server_should_keep_alive(data, raw, res);

        if (!keep_alive)
        {
            /* Also overrides a handler that asked for keep-alive when the server
             * is closing anyway. */
            bb_response_set_header(res, "Connection", "close");
        }
        else
        {
            /* HTTP/1.1 connections are persistent by default; an HTTP/1.0 client
             * has to be told. */
            if (raw->version_major == 1 && raw->version_minor == 0)
            {
                bb_response_set_header(res, "Connection", "keep-alive");
            }

            char hint[48];
            snprintf(hint, sizeof(hint), "timeout=%u", (unsigned)((data->server->keepalive_idle_timeout_ms + 999) / 1000));
            bb_response_set_header(res, "Keep-Alive", hint);
        }
    }

    char *buffer = NULL;
    size_t length = 0;
    if (bb_response_serialize(res, &buffer, &length) != 0)
    {
        // buffer/length are left indeterminate on failure (e.g. the
        // malloc() inside bb_response_serialize() failed) -- don't hand
        // either to bb_connection_buffer_add().
        bb_request_destroy(req);
        bb_response_destroy(res);

        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_ALLOC, "Failed to serialize HTTP response.") };
    }

    if (strcmp(raw->method, "HEAD") == 0)
    {
        char *headers_end = strstr(buffer, "\r\n\r\n");
        if (headers_end)
        {
            length = (size_t)(headers_end - buffer) + 4;
            buffer[length] = '\0';
        }
    }

    if (bb_connection_buffer_add(async_conn->connection, buffer, length) != 0)
    {
        // On failure bb_connection_buffer_add() has already freed
        // `buffer` itself -- don't free it again here.
        bb_request_destroy(req);
        bb_response_destroy(res);

        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_ALLOC, "Failed to buffer HTTP response for write.") };
    }

    // Response is fully built and queued: count it and record how long the
    // request took from its first read (excludes the socket write).
    bb_metrics_observe_request(&data->server->metrics, bb_response_get_status(res), bb_time_monotonic_ms() - data->request_start_ms);

    bb_request_destroy(req);
    bb_response_destroy(res);

    data->keep_alive = keep_alive;
    async_conn->connection->state = BB_CONNECTION_WRITING;

    bb_async_connection_pause_read(async_conn);

    if (_server_create_write_task(data))
    {
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") };
    }

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

static int _server_create_read_task(bb_server_t *server, bb_async_connection_t *async_conn)
{
    bb_server_task_data_t *data = malloc(sizeof(*data));
    if (!data) return 1;

    data->server = server;
    data->async_conn = async_conn;
    data->ws = NULL;
    data->conn_node = NULL;
    data->http_parser = bb_http_parser_create();
    data->parsed_offset = 0;
    data->request_start_ms = -1;
    data->keep_alive = false;
    data->requests_served = 0;
    data->last_activity_ms = bb_time_monotonic_ms();
    if (!data->http_parser)
    {
        free(data);
        return 1;
    }

    /* A connection the server cannot track would never be released at
     * shutdown, so refuse it rather than serve it untracked. */
    if (server->conn_list)
    {
        data->conn_node = bb_conn_list_add(server->conn_list, data);
        if (!data->conn_node)
        {
            bb_http_parser_destroy(data->http_parser);
            free(data);
            return 1;
        }
    }

    bb_async_connection_set_disconnect_callback(async_conn, _server_on_disconnect, data);

    if (BB_FAILED(bb_async_connection_create_read_task(async_conn, _server_read_step, _server_read_error, data)))
    {
        /* The caller destroys async_conn next, which fires its disconnect
         * hook: unhook first, or it runs a cleanup on the data freed here. */
        _server_clear_disconnect_hook(async_conn);
        bb_conn_list_remove(server->conn_list, data->conn_node);
        bb_http_parser_destroy(data->http_parser);
        free(data);
        return 1;
    }

    return 0;
}

// Accept:
void _server_accept_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_server_task_data_t *data = userdata;
    bb_server_t *server = data->server;

    bb_async_connection_t *async_conn;
    while ((async_conn = server->tls_ctx
                ? bb_async_connection_accept_tls(data->async_conn->runtime, data->async_conn->connection->fd, server->tls_ctx)
                : bb_async_connection_accept(data->async_conn->runtime, data->async_conn->connection->fd)))
    {
        bb_metrics_on_connection_accepted(&server->metrics);

        if (_server_create_read_task(data->server, async_conn) != 0)
        {
            bb_async_connection_destroy(async_conn);
        }
    }
}

int bb_server_get_metrics(const bb_server_t *server, bb_metrics_snapshot_t *out)
{
    if (!server || !out)
    {
        return -1;
    }

    bb_metrics_take_snapshot(&server->metrics, bb_conn_list_count(server->conn_list), bb_ws_list_count(server->ws_list), out);
    return 0;
}

bb_error_t bb_server_start(bb_server_t *server)
{
    if (!server)
    {
        return BB_ERROR(BB_ERR_NULL, "No server.");
    }

    if (!server->async_conn || !server->async_conn->connection)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Server has no listening socket.");
    }

    if (server->accept_task)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Server already started.");
    }

    bb_server_task_data_t *data = malloc(sizeof(*data));
    if (!data)
    {
        BB_LOG_ERROR("Failed to allocate server task.\n");
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate server task.");
    }

    data->server = server;
    data->async_conn = server->async_conn;
    data->ws = NULL;
    data->conn_node = NULL;
    data->parsed_offset = 0;
    data->http_parser = NULL;
    data->request_start_ms = -1;
    data->keep_alive = false;
    data->requests_served = 0;
    data->last_activity_ms = 0;
    // not a per-connection object; not tracked in conn_list

    bb_task_t *task = bb_runtime_watch_fd(server->runtime, server->async_conn->connection->fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, _server_accept_task, data);
    if (!task)
    {
        /* Nothing has been published to the server yet, so undoing is just
         * releasing the task data. */
        free(data);
        BB_LOG_ERROR("Failed to register the listening socket with the runtime.\n");
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to register the listening socket with the runtime.");
    }

    server->accept_task_data = data;
    server->accept_task = task;

    BB_LOG_INFO("Blue-Bird async server started.\n");
    return BB_SUCCESS();
}

static void _server_conn_cleanup(void *userdata)
{
    bb_server_task_data_t *data = userdata;

    if (!data)
    {
        return;
    }

    /* Server shutdown destroys the connection itself; don't let its disconnect hook clean up again. */
    _server_clear_disconnect_hook(data->async_conn);

    if (data->http_parser)
    {
        bb_http_parser_destroy(data->http_parser);
        data->http_parser = NULL;
    }

    if (data->ws)
    {
        bb_websocket_destroy(data->ws);
    }
    else if (data->async_conn)
    {
        bb_async_connection_destroy(data->async_conn);
    }

    free(data);
}

void bb_server_destroy(bb_server_t *server)
{
    if (!server)
    {
        return;
    }

    if (server->accept_task)
    {
        bb_runtime_cancel_task(server->runtime, server->accept_task);
        server->accept_task = NULL;
    }

    if (server->keepalive_sweep_task)
    {
        bb_runtime_cancel_task(server->runtime, server->keepalive_sweep_task);
        server->keepalive_sweep_task = NULL;
    }

    if (server->conn_list)
    {
        bb_conn_list_destroy_all(server->conn_list, _server_conn_cleanup);
        server->conn_list = NULL;
    }

    if (server->async_conn)
    {
        bb_async_connection_destroy(server->async_conn);
        server->async_conn = NULL;
    }

    if (server->tls_ctx)
    {
        bb_tls_context_destroy(server->tls_ctx);
        server->tls_ctx = NULL;
    }

    if (server->accept_task_data)
    {
        free(server->accept_task_data);
        server->accept_task_data = NULL;
    }

    bb_route_list_destroy(server->route_list);
    bb_middleware_list_destroy(server->pre_middleware_list);
    bb_middleware_list_destroy(server->post_middleware_list);
    bb_ws_list_destroy(server->ws_list);

    free(server);
}

int bb_server_enable_metrics(bb_server_t *server, const char *path)
{
    if (!server || server->metrics_enabled)
    {
        return -1;
    }

    if (!path)
    {
        path = "/metrics";
    }
    if (path[0] != '/')
    {
        return -1;
    }

    bb_error_t err = bb_route_list_add_http_ctx(server->route_list, "GET", path, _metrics_route_handler, server);
    if (err.code != BB_OK)
    {
        BB_LOG_ERROR("Failed to add metrics route %s: %s\n", path, err.msg);
        return -1;
    }

    server->metrics_enabled = 1;
    return 0;
}

void bb_server_add_route(bb_server_t *server, const char *method, const char *path, bb_http_handler_cb handler)
{
    bb_error_t err = bb_route_list_add_http(server->route_list, method, path, handler);
    if (err.code != BB_OK)
    {
        BB_LOG_ERROR("Failed to add route %s %s: %s\n", method, path, err.msg);
    }
}

void bb_server_add_websocket(bb_server_t *server, const char *path, bb_ws_handler_cb handler)
{
    bb_error_t err = bb_route_list_add_websocket(server->route_list, path, handler);
    if (err.code != BB_OK)
    {
        BB_LOG_ERROR("Failed to add WebSocket route %s: %s\n", path, err.msg);
    }
}

void bb_server_set_websocket_heartbeat(bb_server_t *server, uint32_t interval_ms, uint32_t max_missed_pongs)
{
    if (!server)
    {
        return;
    }

    server->ws_heartbeat_interval_ms = interval_ms;
    server->ws_heartbeat_max_missed_pongs = max_missed_pongs;
}

void bb_server_set_keepalive(bb_server_t *server, uint32_t idle_timeout_ms, uint32_t max_requests)
{
    if (!server)
    {
        return;
    }

    server->keepalive_idle_timeout_ms = idle_timeout_ms;
    server->keepalive_max_requests = max_requests;
}

void bb_server_use_pre_middleware(bb_server_t *server, bb_http_handler_cb mw)
{
    bb_middleware_list_append(server->pre_middleware_list, mw);
}

void bb_server_use_post_middleware(bb_server_t *server, bb_http_handler_cb mw)
{
    bb_middleware_list_append(server->post_middleware_list, mw);
}
