/*
 * bb-loadtest-server: a small, fixed-behaviour server for load and leak testing.
 *
 *   GET  /         -> "ok" (tiny plain-text body; what wrk hammers)
 *   GET  /metrics  -> Prometheus metrics (bb_server_enable_metrics)
 *   WS   /ws       -> echoes text and binary messages (Autobahn target)
 *
 * Usage: bb-loadtest-server [port]      (default 8090, or $BB_LOADTEST_PORT)
 *
 * Prints "bb-loadtest-server listening on port N" once the server is started,
 * so scripts can wait for that line. SIGINT/SIGTERM stop the event loop and the
 * server is destroyed before exit, so Valgrind / LeakSanitizer see a clean
 * shutdown instead of reporting everything still reachable at kill time.
 */

#include <blue-bird/log/log.h>
#include <blue-bird/runtime/runtime.h>
#include <blue-bird/web/server.h>
#include <blue-bird/web/websocket/message.h>
#include <blue-bird/web/websocket/websocket.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 8090
#define STOP_POLL_MS 100

static volatile sig_atomic_t g_stop_requested = 0;

static void on_signal(int sig)
{
    (void) sig;
    g_stop_requested = 1;
}

/* Runs on the event loop: turns the async-signal flag into a clean runtime stop. */
static void stop_check_cb(bb_task_t *task, void *userdata)
{
    (void) task;
    if (g_stop_requested)
    {
        bb_runtime_stop((bb_runtime_t *) userdata);
    }
}

static bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "ok");
    return BB_SUCCESS();
}

/*
 * Echo handler. The public API only offers send_text(const char *), which is
 * NUL-terminated, so a text message containing an embedded NUL byte is echoed
 * truncated at that byte. Binary messages are echoed byte-for-byte.
 */
static bb_error_t echo_handler(bb_websocket_t *ws, const bb_ws_message_t *message)
{
    size_t len = bb_ws_message_get_length(message);
    const void *data = bb_ws_message_get_data(message);

    switch (bb_ws_message_get_type(message))
    {
        case BB_WS_MESSAGE_TEXT:
        {
            char *copy = malloc(len + 1);
            if (!copy)
            {
                return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate echo buffer.");
            }
            if (len > 0)
            {
                memcpy(copy, data, len);
            }
            copy[len] = '\0';

            bb_error_t err = bb_websocket_send_text(ws, copy);
            free(copy);
            return err;
        }
        case BB_WS_MESSAGE_BINARY:
            return bb_websocket_send_binary(ws, data, len);
        default:
            return BB_SUCCESS();
    }
}

static int parse_port(int argc, char **argv)
{
    const char *text = argc > 1 ? argv[1] : getenv("BB_LOADTEST_PORT");
    if (!text || !*text)
    {
        return DEFAULT_PORT;
    }

    char *end = NULL;
    long port = strtol(text, &end, 10);
    if (*end != '\0' || port < 1 || port > 65535)
    {
        fprintf(stderr, "bb-loadtest-server: invalid port '%s'\n", text);
        return -1;
    }
    return (int) port;
}

int main(int argc, char **argv)
{
    int port = parse_port(argc, argv);
    if (port < 0)
    {
        return 2;
    }

    bb_runtime_t *runtime = bb_runtime_create();
    if (!runtime)
    {
        fprintf(stderr, "bb-loadtest-server: failed to create runtime\n");
        return 1;
    }

    bb_server_t *server = bb_server_create_on_runtime(runtime, port);
    if (!server)
    {
        fprintf(stderr, "bb-loadtest-server: failed to create server on port %d\n", port);
        bb_runtime_destroy(runtime);
        return 1;
    }

    bb_server_add_route(server, "GET", "/", root_handler);
    bb_server_add_websocket(server, "/ws", echo_handler);
    if (bb_server_enable_metrics(server, NULL) != 0)
    {
        fprintf(stderr, "bb-loadtest-server: failed to enable metrics\n");
        bb_server_destroy(server);
        bb_runtime_destroy(runtime);
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    bb_runtime_set_interval(runtime, STOP_POLL_MS, stop_check_cb, runtime);

    bb_server_start(server);

    printf("bb-loadtest-server listening on port %d\n", port);
    fflush(stdout);

    bb_runtime_run(runtime);

    bb_server_destroy(server);
    bb_runtime_destroy(runtime);

    printf("bb-loadtest-server stopped\n");
    return 0;
}
