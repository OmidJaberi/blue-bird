/*
 * Server lifecycle and graceful shutdown.
 *
 * Single-threaded: the server's runtime is driven from the test itself and the
 * clients are plain non-blocking TCP sockets, so each scenario is
 * deterministic. "In progress" is arranged by sending part of a request and
 * letting the server read it.
 */
#if !defined(_WIN32)

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"
#include "server_internal.h"
#include "../support/raw_http.h"

#include <blue-bird/utils/platform.h>

#define TEST_PORT 19482

/* ============================================================
 * Fixture
 * ============================================================ */

static bb_error_t ping_handler(bb_request_t *req, bb_response_t *res)
{
    (void)req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "pong");
    return BB_SUCCESS();
}

static bb_error_t ws_handler(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;
    (void)msg;
    return BB_SUCCESS();
}

static void noop_timer(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

typedef struct
{
    bb_runtime_t *runtime;
    bb_server_t *server;

    /* What the shutdown callback saw. */
    int done_calls;
    bb_server_state_t state_in_callback;
    bool in_shutdown_call;
    bool called_inside_shutdown_call;
    bool destroy_in_callback;
} fixture_t;

static void on_done(bb_server_t *server, void *userdata)
{
    fixture_t *f = userdata;

    f->done_calls++;
    f->state_in_callback = bb_server_get_state(server);
    if (f->in_shutdown_call)
    {
        f->called_inside_shutdown_call = true;
    }

    if (f->destroy_in_callback)
    {
        bb_server_destroy(server);
        f->server = NULL;
    }
}

static void fixture_init(fixture_t *f, bool start)
{
    memset(f, 0, sizeof(*f));

    f->runtime = bb_runtime_create();
    BB_ASSERT(f->runtime);
    bb_runtime_set_running(f->runtime);

    /* Keeps bb_runtime_tick() from sleeping out the runtime's idle timeout. */
    BB_ASSERT(bb_runtime_set_interval(f->runtime, 1, noop_timer, NULL) != NULL);

    f->server = bb_server_create_on_runtime(f->runtime, TEST_PORT);
    BB_ASSERT(f->server);

    bb_server_add_route(f->server, "GET", "/ping", ping_handler);
    bb_server_add_websocket(f->server, "/ws", ws_handler);

    if (start)
    {
        BB_ASSERT(!BB_FAILED(bb_server_start(f->server)));
    }
}

static void fixture_destroy(fixture_t *f)
{
    if (f->server)
    {
        bb_server_destroy(f->server);
    }
    bb_runtime_destroy(f->runtime);
}

static bb_error_t begin_shutdown(fixture_t *f, uint32_t grace_ms)
{
    f->in_shutdown_call = true;
    bb_error_t err = bb_server_shutdown(f->server, grace_ms, on_done, f);
    f->in_shutdown_call = false;
    return err;
}

/* Drives the runtime until the shutdown callback has run. */
static bool wait_done(fixture_t *f, int timeout_ms)
{
    int64_t deadline = bb_time_monotonic_ms() + timeout_ms;
    while (f->done_calls == 0 && bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(f->runtime);
    }
    return f->done_calls > 0;
}

/* True if something is listening on the test port. */
static bool port_accepts(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(fd >= 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    BB_ASSERT(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);

    bool connected = connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0;
    close(fd);
    return connected;
}

#define WS_UPGRADE_HEAD \
    "GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" \
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n"

/* ============================================================
 * Tests
 * ============================================================ */

static void test_states_and_arguments(void)
{
    printf("\tTesting lifecycle states and argument errors...\n");

    BB_ASSERT(bb_server_get_state(NULL) == BB_SERVER_STATE_STOPPED);
    BB_ASSERT(bb_server_shutdown(NULL, 100, NULL, NULL).code == BB_ERR_NULL);

    fixture_t f;
    fixture_init(&f, false);
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_CREATED);
    BB_ASSERT(!BB_FAILED(bb_server_start(f.server)));
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_RUNNING);
    fixture_destroy(&f);
}

static void test_shutdown_with_nothing_open(void)
{
    printf("\tTesting shutdown of a server with no connections...\n");

    fixture_t f;
    fixture_init(&f, true);
    BB_ASSERT(port_accepts());

    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 1000)));

    /* Draining, not accepting any more, and the callback has not run yet: it is
     * never called from inside bb_server_shutdown(). */
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_DRAINING);
    BB_ASSERT(f.done_calls == 0);
    BB_ASSERT(!port_accepts());

    BB_ASSERT(wait_done(&f, 2000));
    BB_ASSERT(f.done_calls == 1);
    BB_ASSERT(!f.called_inside_shutdown_call);
    BB_ASSERT(f.state_in_callback == BB_SERVER_STATE_STOPPED);
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_STOPPED);

    /* Exactly once. */
    run_for(f.runtime, 50);
    BB_ASSERT(f.done_calls == 1);

    /* It is over for good. */
    BB_ASSERT(bb_server_shutdown(f.server, 100, NULL, NULL).code == BB_ERR_INTERNAL);
    BB_ASSERT(bb_server_start(f.server).code == BB_ERR_INTERNAL);
    BB_ASSERT(!port_accepts());

    fixture_destroy(&f);
}

static void test_shutdown_twice_is_an_error(void)
{
    printf("\tTesting a second shutdown while draining is refused...\n");

    fixture_t f;
    fixture_init(&f, true);

    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 1000)));
    BB_ASSERT(bb_server_shutdown(f.server, 100, on_done, &f).code == BB_ERR_INTERNAL);

    /* The refused call must not have replaced the first one's callback. */
    BB_ASSERT(wait_done(&f, 2000));
    run_for(f.runtime, 50);
    BB_ASSERT(f.done_calls == 1);

    fixture_destroy(&f);
}

static void test_shutdown_before_start(void)
{
    printf("\tTesting shutdown of a server that was never started...\n");

    fixture_t f;
    fixture_init(&f, false);

    BB_ASSERT(port_accepts()); /* bound at creation */
    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 500)));
    BB_ASSERT(!port_accepts());

    BB_ASSERT(wait_done(&f, 2000));
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_STOPPED);
    BB_ASSERT(bb_server_start(f.server).code == BB_ERR_INTERNAL);

    fixture_destroy(&f);
}

static void test_idle_connections_are_closed(void)
{
    printf("\tTesting connections with nothing in progress are closed at once...\n");

    fixture_t f;
    fixture_init(&f, true);

    /* One that never sent anything, one idle after a keep-alive response. */
    int fresh = raw_client_open(TEST_PORT);
    stream_t fresh_s = {0};

    int idle = raw_client_open(TEST_PORT);
    stream_t idle_s = {0};
    client_send(idle, GET_PING);
    BB_ASSERT(await_responses(f.runtime, idle, &idle_s, 1, false, 2000));
    run_for(f.runtime, 30);
    BB_ASSERT(bb_conn_list_count(f.server->conn_list) == 2);

    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 5000)));

    /* Well inside the grace period: they were not waited for. */
    BB_ASSERT(await_close(f.runtime, fresh, &fresh_s, 500));
    BB_ASSERT(await_close(f.runtime, idle, &idle_s, 500));
    BB_ASSERT(wait_done(&f, 500));
    BB_ASSERT(bb_conn_list_count(f.server->conn_list) == 0);

    close(fresh);
    close(idle);
    fixture_destroy(&f);
}

static void test_in_flight_request_completes(void)
{
    printf("\tTesting a request in progress is allowed to finish...\n");

    fixture_t f;
    fixture_init(&f, true);
    int fd = raw_client_open(TEST_PORT);
    stream_t s = {0};
    response_t r;

    /* Part of a request, read by the server: it is now in progress. */
    client_send(fd, "GET /ping HTTP/1.1\r\nHo");
    run_for(f.runtime, 50);

    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 5000)));

    /* Not closed, and the shutdown is waiting for it. */
    run_for(f.runtime, 100);
    stream_poll(&s, fd);
    BB_ASSERT(!s.eof);
    BB_ASSERT(f.done_calls == 0);
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_DRAINING);

    /* Finish it: answered in full, told the connection ends, then closed. */
    client_send(fd, "st: x\r\n\r\n");
    BB_ASSERT(await_responses(f.runtime, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r));
    BB_ASSERT(r.status == 200 && body_is(&r, "pong"));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(f.runtime, fd, &s, 2000));

    /* And only then is the shutdown complete: no grace period left to wait out. */
    BB_ASSERT(wait_done(&f, 500));
    BB_ASSERT(f.state_in_callback == BB_SERVER_STATE_STOPPED);

    close(fd);
    fixture_destroy(&f);
}

static void test_websocket_upgrade_is_refused_while_draining(void)
{
    printf("\tTesting a WebSocket upgrade that arrives during a shutdown is refused...\n");

    fixture_t f;
    fixture_init(&f, true);
    int fd = raw_client_open(TEST_PORT);
    stream_t s = {0};
    response_t r;

    /* An upgrade request, incomplete when the shutdown begins. */
    client_send(fd, WS_UPGRADE_HEAD);
    run_for(f.runtime, 50);
    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 5000)));

    client_send(fd, "\r\n");
    BB_ASSERT(await_responses(f.runtime, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r));
    BB_ASSERT(r.status == 503);
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(f.runtime, fd, &s, 2000));
    BB_ASSERT(wait_done(&f, 500));
    BB_ASSERT(bb_ws_list_count(f.server->ws_list) == 0);

    close(fd);
    fixture_destroy(&f);
}

static void test_grace_period_is_enforced(void)
{
    printf("\tTesting what is still open when the grace period ends is closed...\n");

    fixture_t f;
    fixture_init(&f, true);
    int fd = raw_client_open(TEST_PORT);
    stream_t s = {0};

    /* A request that never completes. */
    client_send(fd, "GET /ping HTTP/1.1\r\nHo");
    run_for(f.runtime, 50);

    int64_t started = bb_time_monotonic_ms();
    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 300)));

    /* Held open until the deadline... */
    run_for(f.runtime, 150);
    stream_poll(&s, fd);
    BB_ASSERT(!s.eof);
    BB_ASSERT(f.done_calls == 0);

    /* ... then cut. */
    BB_ASSERT(await_close(f.runtime, fd, &s, 2000));
    BB_ASSERT(wait_done(&f, 1000));
    int64_t elapsed = bb_time_monotonic_ms() - started;
    BB_ASSERT(elapsed >= 280);
    BB_ASSERT(elapsed < 1500);
    BB_ASSERT(bb_conn_list_count(f.server->conn_list) == 0);
    BB_ASSERT(f.state_in_callback == BB_SERVER_STATE_STOPPED);

    close(fd);
    fixture_destroy(&f);
}

static void test_zero_grace_closes_everything_at_once(void)
{
    printf("\tTesting a grace period of 0...\n");

    fixture_t f;
    fixture_init(&f, true);
    int fd = raw_client_open(TEST_PORT);
    stream_t s = {0};

    client_send(fd, "GET /ping HTTP/1.1\r\nHo");
    run_for(f.runtime, 50);

    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 0)));
    BB_ASSERT(await_close(f.runtime, fd, &s, 500));
    BB_ASSERT(wait_done(&f, 500));

    close(fd);
    fixture_destroy(&f);
}

static void test_websocket_sessions_are_told_goodbye(void)
{
    printf("\tTesting WebSocket sessions get a 1001 close frame...\n");

    fixture_t f;
    fixture_init(&f, true);
    int fd = raw_client_open(TEST_PORT);
    stream_t s = {0};
    response_t r;

    client_send(fd, WS_UPGRADE_HEAD "\r\n");
    BB_ASSERT(await_responses(f.runtime, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r) && r.status == 101);

    int64_t deadline = bb_time_monotonic_ms() + 2000;
    while (bb_ws_list_count(f.server->ws_list) != 1 && bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(f.runtime);
    }
    BB_ASSERT(bb_ws_list_count(f.server->ws_list) == 1);

    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 5000)));

    /* A close frame (FIN + opcode 8) carrying status 1001, then the end. */
    size_t after_upgrade = r.total;
    BB_ASSERT(await_close(f.runtime, fd, &s, 2000));
    BB_ASSERT(s.len >= after_upgrade + 4);

    const unsigned char *frame = (const unsigned char *)s.data + after_upgrade;
    BB_ASSERT(frame[0] == 0x88);
    BB_ASSERT(frame[1] >= 2);
    BB_ASSERT(((frame[2] << 8) | frame[3]) == 1001);

    /* The session is gone, so the shutdown is done without waiting out the grace. */
    BB_ASSERT(wait_done(&f, 500));
    BB_ASSERT(bb_ws_list_count(f.server->ws_list) == 0);

    close(fd);
    fixture_destroy(&f);
}

static void test_destroy_during_shutdown(void)
{
    printf("\tTesting the server can be destroyed in the middle of a shutdown...\n");

    fixture_t f;
    fixture_init(&f, true);
    int fd = raw_client_open(TEST_PORT);
    stream_t s = {0};

    client_send(fd, "GET /ping HTTP/1.1\r\nHo");
    run_for(f.runtime, 50);
    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 5000)));

    bb_server_destroy(f.server);
    f.server = NULL;

    /* The connection is dropped, and the shutdown callback never runs. */
    BB_ASSERT(await_close(f.runtime, fd, &s, 500));
    run_for(f.runtime, 100);
    BB_ASSERT(f.done_calls == 0);

    close(fd);
    fixture_destroy(&f);
}

static void test_callback_may_destroy_the_server(void)
{
    printf("\tTesting the shutdown callback may destroy the server...\n");

    fixture_t f;
    fixture_init(&f, true);
    f.destroy_in_callback = true;

    BB_ASSERT(!BB_FAILED(begin_shutdown(&f, 1000)));
    BB_ASSERT(wait_done(&f, 2000));
    BB_ASSERT(f.done_calls == 1);
    BB_ASSERT(f.server == NULL);

    /* Nothing of the destroyed server is left scheduled. */
    run_for(f.runtime, 100);
    BB_ASSERT(f.done_calls == 1);

    fixture_destroy(&f);
}

static void test_destroy_without_shutdown_still_works(void)
{
    printf("\tTesting plain destroy of a running server with open connections...\n");

    fixture_t f;
    fixture_init(&f, true);
    int fd = raw_client_open(TEST_PORT);
    stream_t s = {0};
    client_send(fd, GET_PING);
    BB_ASSERT(await_responses(f.runtime, fd, &s, 1, false, 2000));

    bb_server_destroy(f.server);
    f.server = NULL;

    BB_ASSERT(await_close(f.runtime, fd, &s, 500));
    BB_ASSERT(!port_accepts());

    close(fd);
    fixture_destroy(&f);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    BB_ASSERT(bb_platform_net_init() == 0);

    printf("Running server shutdown tests...\n");

    test_states_and_arguments();
    test_shutdown_with_nothing_open();
    test_shutdown_twice_is_an_error();
    test_shutdown_before_start();
    test_idle_connections_are_closed();
    test_in_flight_request_completes();
    test_websocket_upgrade_is_refused_while_draining();
    test_grace_period_is_enforced();
    test_zero_grace_closes_everything_at_once();
    test_websocket_sessions_are_told_goodbye();
    test_destroy_during_shutdown();
    test_callback_may_destroy_the_server();
    test_destroy_without_shutdown_still_works();

    printf("All server shutdown tests passed.\n");
    return 0;
}

#else /* _WIN32 */

/* These tests drive the server through raw POSIX sockets. */
#include <stdio.h>

int main(void)
{
    printf("Skipping: POSIX-socket test, not built on Windows.\n");
    return 0;
}

#endif /* !_WIN32 */
