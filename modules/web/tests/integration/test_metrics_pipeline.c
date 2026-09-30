#include "blue-bird/web/server.h"
#include "blue-bird/utils/platform.h" /* portable sockets, bb_usleep(); pulls in winsock2 on Windows */

#include <blue-bird/error/assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define TEST_PORT 18099

static bb_runtime_t *rt;

static bb_error_t ok_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "hi");
    return BB_SUCCESS();
}

static bb_error_t ws_handler(bb_websocket_t *ws, const bb_ws_message_t *message)
{
    (void) ws;
    (void) message;
    return BB_SUCCESS();
}

static void *server_thread(void *arg)
{
    (void) arg;
    bb_runtime_run(rt);
    return NULL;
}

/* Receive timeout so a misbehaving server fails the test instead of hanging it. */
static void set_recv_timeout_ms(bb_socket_t fd, int ms)
{
#ifdef _WIN32
    DWORD tv = (DWORD) ms;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(tv));
#else
    struct timeval tv = { .tv_sec = ms / 1000, .tv_usec = (ms % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

static bb_socket_t client_connect(void)
{
    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(!bb_socket_is_invalid(fd));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    BB_ASSERT(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);

    BB_ASSERT(connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == 0);
    set_recv_timeout_ms(fd, 2000);
    return fd;
}

static void send_all(bb_socket_t fd, const char *s)
{
    int len = (int) strlen(s);
    BB_ASSERT(send(fd, s, len, MSG_NOSIGNAL) == len);
}

static void http_get(const char *path)
{
    bb_socket_t fd = client_connect();
    char req[256];
    snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", path);
    send_all(fd, req);

    char buf[1024];
    while (recv(fd, buf, (int) sizeof(buf), 0) > 0) {}
    bb_socket_close(fd);
}

/* Performs the upgrade handshake and returns the still-open socket. */
static bb_socket_t ws_open(const char *path)
{
    bb_socket_t fd = client_connect();
    char req[512];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n",
             path);
    send_all(fd, req);

    char buf[1024];
    int n = recv(fd, buf, (int) sizeof(buf) - 1, 0);
    BB_ASSERT(n > 0);
    buf[n] = '\0';
    BB_ASSERT(strstr(buf, "101") != NULL);
    return fd;
}

static void test_null_args(bb_server_t *server)
{
    printf("Testing bb_server_get_metrics NULL args...\n");
    bb_metrics_snapshot_t snap;
    BB_ASSERT(bb_server_get_metrics(NULL, &snap) == -1);
    BB_ASSERT(bb_server_get_metrics(server, NULL) == -1);
}

int main(void)
{
    rt = bb_runtime_create();
    bb_server_t *server = bb_server_create_on_runtime(rt, TEST_PORT);
    BB_ASSERT(server != NULL);
    bb_server_add_route(server, "GET", "/ok", ok_handler);
    bb_server_add_websocket(server, "/ws", ws_handler);
    bb_server_start(server);

    test_null_args(server);

    pthread_t th;
    pthread_create(&th, NULL, server_thread, NULL);
    while (!bb_runtime_is_running(rt)) bb_usleep(1000);

    printf("Testing metrics after HTTP traffic, an idle connection and a WebSocket session...\n");
    for (int i = 0; i < 5; i++) http_get("/ok");
    for (int i = 0; i < 2; i++) http_get("/nope");

    bb_socket_t idle_fd = client_connect(); /* connected, never sends a request */
    bb_socket_t ws_fd = ws_open("/ws");     /* upgraded, stays open */
    bb_usleep(200000);

    /* Stop the loop before reading: bb_server_get_metrics() is single-thread only. */
    bb_runtime_stop(rt);
    pthread_join(th, NULL);

    bb_metrics_snapshot_t snap;
    BB_ASSERT(bb_server_get_metrics(server, &snap) == 0);

    const bb_metrics_t *m = &snap.counters;
    printf("accepted=%llu requests=%llu 2xx=%llu 4xx=%llu 1xx=%llu ws_opened=%llu active_conns=%llu active_ws=%llu\n",
           (unsigned long long) m->connections_accepted_total,
           (unsigned long long) m->requests_total,
           (unsigned long long) m->responses_by_class[2],
           (unsigned long long) m->responses_by_class[4],
           (unsigned long long) m->responses_by_class[1],
           (unsigned long long) m->ws_sessions_opened_total,
           (unsigned long long) snap.active_connections,
           (unsigned long long) snap.active_ws_sessions);

    /* 7 HTTP + 1 idle + 1 WebSocket upgrade */
    BB_ASSERT(m->connections_accepted_total == 9);
    /* 7 HTTP responses + the 101 upgrade response; the idle connection sent nothing */
    BB_ASSERT(m->requests_total == 8);
    BB_ASSERT(m->responses_by_class[2] == 5);
    BB_ASSERT(m->responses_by_class[4] == 2);
    BB_ASSERT(m->responses_by_class[1] == 1);
    BB_ASSERT(m->latency_count == 8);

    uint64_t bucket_total = 0;
    for (size_t i = 0; i < BB_METRICS_LATENCY_BUCKETS; i++)
    {
        bucket_total += m->latency_bucket[i];
    }
    BB_ASSERT(bucket_total == m->latency_count);

    BB_ASSERT(m->ws_sessions_opened_total == 1);
    BB_ASSERT(snap.active_ws_sessions == 1);
    /* Only the idle connection is still an HTTP connection; the upgraded one moved to the WS list. */
    BB_ASSERT(snap.active_connections == 1);

    bb_socket_close(idle_fd);
    bb_socket_close(ws_fd);
    bb_server_destroy(server);
    bb_runtime_destroy(rt);

    printf("All metrics pipeline tests passed.\n");
    return 0;
}
