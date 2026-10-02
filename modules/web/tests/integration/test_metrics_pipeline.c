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

/* Stand-in for an auth middleware: rejects any request carrying X-Block. */
static bb_error_t block_mw(bb_request_t *req, bb_response_t *res)
{
    (void) res;
    if (bb_request_get_header(req, "X-Block"))
    {
        return BB_ERROR(BB_ERR_BAD_REQUEST, "blocked by test middleware");
    }
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

/* GET with optional extra header lines; returns the full raw response (NUL-terminated). */
static size_t http_get_raw(const char *path, const char *extra_headers, char *out, size_t cap)
{
    bb_socket_t fd = client_connect();
    char req[512];
    snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: x\r\n%sConnection: close\r\n\r\n",
             path, extra_headers ? extra_headers : "");
    send_all(fd, req);

    size_t total = 0;
    for (;;)
    {
        if (total + 1 >= cap)
        {
            break;
        }
        int n = recv(fd, out + total, (int) (cap - total - 1), 0);
        if (n <= 0)
        {
            break;
        }
        total += (size_t) n;
    }
    out[total] = '\0';
    bb_socket_close(fd);
    return total;
}

static void http_get(const char *path)
{
    char buf[1024];
    http_get_raw(path, NULL, buf, sizeof(buf));
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
    BB_ASSERT(strstr(buf, "Connection: close") == NULL); /* the upgrade keeps the connection open */
    return fd;
}

static void test_null_args(bb_server_t *server)
{
    printf("Testing bb_server_get_metrics NULL args...\n");
    bb_metrics_snapshot_t snap;
    BB_ASSERT(bb_server_get_metrics(NULL, &snap) == -1);
    BB_ASSERT(bb_server_get_metrics(server, NULL) == -1);

    printf("Testing bb_server_enable_metrics argument checks...\n");
    BB_ASSERT(bb_server_enable_metrics(NULL, "/metrics") == -1);
    BB_ASSERT(bb_server_enable_metrics(server, "metrics") == -1); /* must start with '/' */
    BB_ASSERT(bb_server_enable_metrics(server, "") == -1);
}

static void test_enable_twice(bb_server_t *server)
{
    printf("Testing bb_server_enable_metrics only once per server...\n");
    BB_ASSERT(bb_server_enable_metrics(server, NULL) == -1);
    BB_ASSERT(bb_server_enable_metrics(server, "/other") == -1);
}

int main(void)
{
    rt = bb_runtime_create();
    bb_server_t *server = bb_server_create_on_runtime(rt, TEST_PORT);
    BB_ASSERT(server != NULL);
    bb_server_add_route(server, "GET", "/ok", ok_handler);
    bb_server_add_websocket(server, "/ws", ws_handler);
    bb_server_use_pre_middleware(server, block_mw);
    BB_ASSERT(bb_server_enable_metrics(server, NULL) == 0); /* default path: /metrics */
    test_enable_twice(server);
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

    printf("Testing GET /metrics scrape...\n");
    static char scrape[8192];
    size_t scrape_len = http_get_raw("/metrics", NULL, scrape, sizeof(scrape));
    BB_ASSERT(scrape_len > 0 && scrape_len + 1 < sizeof(scrape)); /* not truncated */
    BB_ASSERT(strstr(scrape, "HTTP/1.1 200") != NULL);
    BB_ASSERT(strstr(scrape, "text/plain; version=0.0.4") != NULL);
    BB_ASSERT(strstr(scrape, "Connection: close") != NULL); /* the server closes after every HTTP response */

    /* The scrape sees state as of just before it finishes: its own connection is
     * accepted and open, but its own request isn't counted yet. */
    BB_ASSERT(strstr(scrape, "bluebird_http_connections_active 2\n") != NULL); /* idle + scrape */
    BB_ASSERT(strstr(scrape, "bluebird_websocket_sessions_active 1\n") != NULL);
    BB_ASSERT(strstr(scrape, "bluebird_connections_accepted_total 10\n") != NULL);
    BB_ASSERT(strstr(scrape, "bluebird_websocket_sessions_opened_total 1\n") != NULL);
    BB_ASSERT(strstr(scrape, "bluebird_http_requests_total{status_class=\"1xx\"} 1\n") != NULL);
    BB_ASSERT(strstr(scrape, "bluebird_http_requests_total{status_class=\"2xx\"} 5\n") != NULL);
    BB_ASSERT(strstr(scrape, "bluebird_http_requests_total{status_class=\"4xx\"} 2\n") != NULL);
    BB_ASSERT(strstr(scrape, "_bucket{le=\"+Inf\"} 8\n") != NULL);
    BB_ASSERT(strstr(scrape, "bluebird_http_request_duration_seconds_count 8\n") != NULL);

    printf("Testing pre-middleware protects /metrics...\n");
    char blocked[2048];
    http_get_raw("/metrics", "X-Block: 1\r\n", blocked, sizeof(blocked));
    BB_ASSERT(strstr(blocked, "bluebird_") == NULL);
    BB_ASSERT(strstr(blocked, "HTTP/1.1 200") == NULL);

    bb_usleep(100000);

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

    /* 7 HTTP + 1 idle + 1 WebSocket upgrade + 1 scrape + 1 blocked scrape */
    BB_ASSERT(m->connections_accepted_total == 11);
    /* 7 HTTP + the 101 upgrade + the scrape; the idle connection sent nothing and
     * the blocked scrape was dropped by the middleware without a response */
    BB_ASSERT(m->requests_total == 9);
    BB_ASSERT(m->responses_by_class[2] == 6);
    BB_ASSERT(m->responses_by_class[4] == 2);
    BB_ASSERT(m->responses_by_class[1] == 1);
    BB_ASSERT(m->latency_count == 9);

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
