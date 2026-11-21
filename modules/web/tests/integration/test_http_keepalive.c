/*
 * HTTP keep-alive (persistent connections).
 *
 * Single-threaded: the server's runtime is driven from the test itself and the
 * client is a plain non-blocking TCP socket, so every exchange is
 * deterministic and nothing races a server thread.
 *
 * A response is read by its framing (status line, headers, Content-Length),
 * never by waiting for the connection to close, which is what makes it
 * possible to tell "answered and still open" from "answered and closed".
 */
#if !defined(_WIN32)

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"
#include "blue-bird/web/http/message.h"
#include "server_internal.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>
#include <blue-bird/utils/time.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define TEST_PORT 19480

/* ============================================================
 * Routes
 * ============================================================ */

static bb_error_t ping_handler(bb_request_t *req, bb_response_t *res)
{
    (void)req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "pong");
    return BB_SUCCESS();
}

static bb_error_t id_handler(bb_request_t *req, bb_response_t *res)
{
    char body[64];
    const char *id = bb_request_get_param(req, "id");
    snprintf(body, sizeof(body), "id: %s", id ? id : "?");
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, body);
    return BB_SUCCESS();
}

static bb_error_t echo_handler(bb_request_t *req, bb_response_t *res)
{
    bb_http_message_t *msg = bb_request_get_message(req);
    const char *body = bb_message_get_body(msg);
    size_t len = (size_t)bb_message_get_body_len(msg);

    char *out = malloc(len + 16);
    BB_ASSERT(out);
    snprintf(out, len + 16, "echo[%zu]: %s", len, body ? body : "");

    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, out);
    free(out);
    return BB_SUCCESS();
}

/* A handler that sets a body on a response that must not have one. */
static bb_error_t no_content_handler(bb_request_t *req, bb_response_t *res)
{
    (void)req;
    bb_response_set_status(res, 204);
    bb_response_set_body(res, "this must never reach the wire");
    return BB_SUCCESS();
}

static bb_error_t close_handler(bb_request_t *req, bb_response_t *res)
{
    (void)req;
    bb_response_set_header(res, "Connection", "close");
    bb_response_set_body(res, "bye");
    return BB_SUCCESS();
}

/* A handler that insists on keep-alive; the server may still close. */
static bb_error_t insist_keepalive_handler(bb_request_t *req, bb_response_t *res)
{
    (void)req;
    bb_response_set_header(res, "Connection", "keep-alive");
    bb_response_set_body(res, "insist");
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

/* ============================================================
 * Fixture
 * ============================================================ */

typedef struct
{
    bb_runtime_t *runtime;
    bb_server_t *server;
} fixture_t;

static void fixture_init(fixture_t *f, uint32_t idle_timeout_ms, uint32_t max_requests)
{
    f->runtime = bb_runtime_create();
    BB_ASSERT(f->runtime);
    bb_runtime_set_running(f->runtime);

    /* Keeps bb_runtime_tick() from sleeping out the runtime's idle timeout. */
    BB_ASSERT(bb_runtime_set_interval(f->runtime, 1, noop_timer, NULL) != NULL);

    f->server = bb_server_create_on_runtime(f->runtime, TEST_PORT);
    BB_ASSERT(f->server);

    bb_server_set_keepalive(f->server, idle_timeout_ms, max_requests);

    bb_server_add_route(f->server, "GET", "/ping", ping_handler);
    bb_server_add_route(f->server, "HEAD", "/ping", ping_handler);
    bb_server_add_route(f->server, "GET", "/id/:id", id_handler);
    bb_server_add_route(f->server, "POST", "/echo", echo_handler);
    bb_server_add_route(f->server, "GET", "/no_content", no_content_handler);
    bb_server_add_route(f->server, "GET", "/close", close_handler);
    bb_server_add_route(f->server, "GET", "/insist", insist_keepalive_handler);
    bb_server_add_websocket(f->server, "/ws", ws_handler);

    BB_ASSERT(!BB_FAILED(bb_server_start(f->server)));
}

static void fixture_destroy(fixture_t *f)
{
    bb_server_destroy(f->server);
    bb_runtime_destroy(f->runtime);
}

static size_t open_connections(fixture_t *f)
{
    return bb_conn_list_count(f->server->conn_list);
}

/* ============================================================
 * Client
 * ============================================================ */

static int client_open(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(fd >= 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    BB_ASSERT(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    BB_ASSERT(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    BB_ASSERT(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);

    return fd;
}

static void client_send(int fd, const char *data)
{
    size_t len = strlen(data);
    BB_ASSERT(send(fd, data, len, 0) == (ssize_t)len);
}

typedef struct
{
    char data[16384];
    size_t len;
    bool eof;
} stream_t;

/* Pulls whatever the server has sent so far; returns false once the peer has closed. */
static void stream_poll(stream_t *s, int fd)
{
    if (s->eof)
    {
        return;
    }

    for (;;)
    {
        ssize_t n = recv(fd, s->data + s->len, sizeof(s->data) - 1 - s->len, 0);
        if (n > 0)
        {
            s->len += (size_t)n;
            s->data[s->len] = '\0';
            continue;
        }
        if (n == 0)
        {
            s->eof = true;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            s->eof = true; /* reset */
        }
        return;
    }
}

typedef struct
{
    int status;
    const char *headers; /* start of the header block, after the status line */
    size_t headers_len;
    const char *body;
    size_t body_len;
    size_t total;        /* bytes this response occupies in the stream */
} response_t;

static const char *find_header(const response_t *r, const char *name)
{
    size_t name_len = strlen(name);
    const char *p = r->headers;
    const char *end = r->headers + r->headers_len;

    while (p < end)
    {
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        if (!eol)
        {
            break;
        }
        if ((size_t)(eol - p) > name_len + 1 && strncasecmp(p, name, name_len) == 0 && p[name_len] == ':')
        {
            p += name_len + 1;
            while (*p == ' ')
            {
                p++;
            }
            return p;
        }
        p = eol + 1;
    }
    return NULL;
}

static bool has_header_value(const response_t *r, const char *name, const char *prefix)
{
    const char *v = find_header(r, name);
    return v && strncasecmp(v, prefix, strlen(prefix)) == 0;
}

/* Parses the index-th response in the stream by its framing. `head` marks a
 * response to HEAD, whose Content-Length describes a body that is not sent.
 * Returns false while that response is still incomplete. */
static bool stream_response(const stream_t *s, size_t index, bool head, response_t *out)
{
    size_t offset = 0;

    for (size_t i = 0;; i++)
    {
        if (offset >= s->len)
        {
            return false;
        }

        const char *start = s->data + offset;
        const char *headers_end = strstr(start, "\r\n\r\n");
        if (!headers_end)
        {
            return false;
        }

        response_t r;
        memset(&r, 0, sizeof(r));
        BB_ASSERT(strncmp(start, "HTTP/1.1 ", 9) == 0);
        r.status = atoi(start + 9);

        const char *line_end = strstr(start, "\r\n");
        r.headers = line_end + 2;
        r.headers_len = (size_t)(headers_end + 2 - r.headers);

        size_t content_length = 0;
        const char *cl = find_header(&r, "Content-Length");
        if (cl)
        {
            content_length = (size_t)strtoul(cl, NULL, 10);
        }
        if (head)
        {
            content_length = 0;
        }

        r.body = headers_end + 4;
        r.body_len = content_length;
        size_t header_bytes = (size_t)(headers_end + 4 - start);
        r.total = header_bytes + content_length;

        if (offset + r.total > s->len)
        {
            return false;
        }

        if (i == index)
        {
            *out = r;
            return true;
        }
        offset += r.total;
    }
}

static bool body_is(const response_t *r, const char *expected)
{
    return r->body_len == strlen(expected) && memcmp(r->body, expected, r->body_len) == 0;
}

/* Drives the server until the stream holds `count` complete responses. */
static bool await_responses(fixture_t *f, int fd, stream_t *s, size_t count, bool head, int timeout_ms)
{
    int64_t deadline = bb_time_monotonic_ms() + timeout_ms;
    response_t r;

    for (;;)
    {
        bb_runtime_tick(f->runtime);
        stream_poll(s, fd);

        if (stream_response(s, count - 1, head, &r))
        {
            return true;
        }
        if (s->eof || bb_time_monotonic_ms() > deadline)
        {
            return false;
        }
    }
}

/* Drives the server for `ms`; true if the peer closed in that time. */
static bool await_close(fixture_t *f, int fd, stream_t *s, int ms)
{
    int64_t deadline = bb_time_monotonic_ms() + ms;

    while (bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(f->runtime);
        stream_poll(s, fd);
        if (s->eof)
        {
            return true;
        }
    }
    return false;
}

/* Drives the server for `ms` without anything else happening. */
static void run_for(fixture_t *f, int ms)
{
    int64_t deadline = bb_time_monotonic_ms() + ms;
    while (bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(f->runtime);
    }
}

#define GET_PING "GET /ping HTTP/1.1\r\nHost: x\r\n\r\n"
#define GET_ID(n) "GET /id/" #n " HTTP/1.1\r\nHost: x\r\n\r\n"

/* ============================================================
 * Tests
 * ============================================================ */

static void test_two_requests_one_connection(void)
{
    printf("\tTesting two requests on one connection...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r));
    BB_ASSERT(r.status == 200 && body_is(&r, "pong"));

    /* HTTP/1.1 is persistent by default: no "Connection: close", and the
     * server says how long it will wait. */
    BB_ASSERT(find_header(&r, "Connection") == NULL);
    BB_ASSERT(has_header_value(&r, "Keep-Alive", "timeout=5"));
    BB_ASSERT(open_connections(&f) == 1);

    client_send(fd, GET_ID(42));
    BB_ASSERT(await_responses(&f, fd, &s, 2, false, 2000));
    BB_ASSERT(stream_response(&s, 1, false, &r));
    BB_ASSERT(r.status == 200 && body_is(&r, "id: 42"));

    BB_ASSERT(!s.eof);
    BB_ASSERT(open_connections(&f) == 1);

    close(fd);
    fixture_destroy(&f);
}

static void test_connection_close_requested(void)
{
    printf("\tTesting \"Connection: close\" from the client...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, "GET /ping HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r));
    BB_ASSERT(r.status == 200 && body_is(&r, "pong"));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(&f, fd, &s, 2000));

    /* Token lists and any capitalisation count. */
    int fd2 = client_open();
    stream_t s2 = {0};
    client_send(fd2, "GET /ping HTTP/1.1\r\nHost: x\r\nconnection: Keep-Alive, CLOSE\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd2, &s2, 1, false, 2000));
    BB_ASSERT(stream_response(&s2, 0, false, &r));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(&f, fd2, &s2, 2000));

    close(fd);
    close(fd2);
    fixture_destroy(&f);
}

static void test_http10(void)
{
    printf("\tTesting HTTP/1.0 clients...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    response_t r;

    /* HTTP/1.0 is not persistent unless the client asks. */
    int fd = client_open();
    stream_t s = {0};
    client_send(fd, "GET /ping HTTP/1.0\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(&f, fd, &s, 2000));
    close(fd);

    /* Asking for keep-alive gets it, and the response has to confirm it. */
    int fd2 = client_open();
    stream_t s2 = {0};
    client_send(fd2, "GET /ping HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd2, &s2, 1, false, 2000));
    BB_ASSERT(stream_response(&s2, 0, false, &r));
    BB_ASSERT(has_header_value(&r, "Connection", "keep-alive"));

    client_send(fd2, "GET /id/7 HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd2, &s2, 2, false, 2000));
    BB_ASSERT(stream_response(&s2, 1, false, &r));
    BB_ASSERT(body_is(&r, "id: 7"));
    BB_ASSERT(!s2.eof);

    close(fd2);
    fixture_destroy(&f);
}

static void test_pipelined_requests_in_order(void)
{
    printf("\tTesting pipelined requests are answered in order...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, GET_ID(1) GET_ID(2) GET_ID(3) GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 4, false, 3000));

    BB_ASSERT(stream_response(&s, 0, false, &r) && body_is(&r, "id: 1"));
    BB_ASSERT(stream_response(&s, 1, false, &r) && body_is(&r, "id: 2"));
    BB_ASSERT(stream_response(&s, 2, false, &r) && body_is(&r, "id: 3"));
    BB_ASSERT(stream_response(&s, 3, false, &r) && body_is(&r, "pong"));

    /* Nothing more than the four responses, and the connection is still usable. */
    BB_ASSERT(!stream_response(&s, 4, false, &r));
    BB_ASSERT(!s.eof);

    client_send(fd, GET_ID(5));
    BB_ASSERT(await_responses(&f, fd, &s, 5, false, 2000));
    BB_ASSERT(stream_response(&s, 4, false, &r) && body_is(&r, "id: 5"));

    close(fd);
    fixture_destroy(&f);
}

static void test_pipelined_request_arrives_in_pieces(void)
{
    printf("\tTesting a pipelined request that arrives in pieces...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    /* A whole request plus the first half of the next one. */
    client_send(fd, GET_ID(1) "GET /id/2 HTTP/1.1\r\nHo");
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r) && body_is(&r, "id: 1"));

    /* The half-request must not have been answered, mangled or dropped. */
    run_for(&f, 50);
    stream_poll(&s, fd);
    BB_ASSERT(!stream_response(&s, 1, false, &r));
    BB_ASSERT(!s.eof);

    client_send(fd, "st: x\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd, &s, 2, false, 2000));
    BB_ASSERT(stream_response(&s, 1, false, &r) && body_is(&r, "id: 2"));

    close(fd);
    fixture_destroy(&f);
}

static void test_request_bodies_keep_framing(void)
{
    printf("\tTesting request bodies do not disturb the next request...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd,
        "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 11\r\n\r\nhello world"
        "POST /echo HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
        "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n"
        GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 3, false, 3000));

    BB_ASSERT(stream_response(&s, 0, false, &r) && body_is(&r, "echo[11]: hello world"));
    BB_ASSERT(stream_response(&s, 1, false, &r) && body_is(&r, "echo[11]: hello world"));
    BB_ASSERT(stream_response(&s, 2, false, &r) && body_is(&r, "pong"));
    BB_ASSERT(!s.eof);

    close(fd);
    fixture_destroy(&f);
}

static void test_head_response_has_no_body(void)
{
    printf("\tTesting a HEAD response carries no body...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, "HEAD /ping HTTP/1.1\r\nHost: x\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd, &s, 1, true, 2000));
    BB_ASSERT(stream_response(&s, 0, true, &r));
    BB_ASSERT(r.status == 200);

    /* Content-Length is what a GET would return, but no body follows it. If
     * body bytes did follow, the client would read them as the start of the
     * next response and every later answer on the connection would be garbage. */
    BB_ASSERT(has_header_value(&r, "Content-Length", "4"));
    run_for(&f, 50);
    stream_poll(&s, fd);
    BB_ASSERT(s.len == r.total);
    BB_ASSERT(!s.eof);

    /* The connection is intact: the next request is answered correctly. */
    stream_t next = {0};
    client_send(fd, GET_ID(9));
    BB_ASSERT(await_responses(&f, fd, &next, 1, false, 2000));
    BB_ASSERT(stream_response(&next, 0, false, &r) && body_is(&r, "id: 9"));

    close(fd);
    fixture_destroy(&f);
}

static void test_no_body_statuses(void)
{
    printf("\tTesting a 204 response carries no body...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, "GET /no_content HTTP/1.1\r\nHost: x\r\n\r\n" GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 2, false, 2000));

    BB_ASSERT(stream_response(&s, 0, false, &r));
    BB_ASSERT(r.status == 204 && r.body_len == 0);
    BB_ASSERT(strstr(s.data, "this must never reach the wire") == NULL);
    BB_ASSERT(stream_response(&s, 1, false, &r) && body_is(&r, "pong"));
    BB_ASSERT(!s.eof);

    close(fd);
    fixture_destroy(&f);
}

static void test_handler_can_close(void)
{
    printf("\tTesting a handler that sets \"Connection: close\"...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, "GET /close HTTP/1.1\r\nHost: x\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r) && body_is(&r, "bye"));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(&f, fd, &s, 2000));
    close(fd);

    /* A handler cannot hold a connection open that the client asked to close:
     * the header must say what the server is actually going to do. */
    int fd2 = client_open();
    stream_t s2 = {0};
    client_send(fd2, "GET /insist HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    BB_ASSERT(await_responses(&f, fd2, &s2, 1, false, 2000));
    BB_ASSERT(stream_response(&s2, 0, false, &r));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(&f, fd2, &s2, 2000));
    close(fd2);

    fixture_destroy(&f);
}

static void test_max_requests_per_connection(void)
{
    printf("\tTesting the per-connection request limit...\n");

    fixture_t f;
    fixture_init(&f, 5000, 3);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    for (size_t i = 1; i <= 3; i++)
    {
        client_send(fd, GET_PING);
        BB_ASSERT(await_responses(&f, fd, &s, i, false, 2000));
    }

    BB_ASSERT(stream_response(&s, 0, false, &r) && find_header(&r, "Connection") == NULL);
    BB_ASSERT(stream_response(&s, 1, false, &r) && find_header(&r, "Connection") == NULL);

    /* The last request the connection will serve says so, then it closes. */
    BB_ASSERT(stream_response(&s, 2, false, &r));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(await_close(&f, fd, &s, 2000));

    close(fd);
    fixture_destroy(&f);
}

static void test_keepalive_can_be_disabled(void)
{
    printf("\tTesting keep-alive can be turned off...\n");

    fixture_t f;
    fixture_init(&f, 0, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(stream_response(&s, 0, false, &r) && body_is(&r, "pong"));
    BB_ASSERT(has_header_value(&r, "Connection", "close"));
    BB_ASSERT(find_header(&r, "Keep-Alive") == NULL);
    BB_ASSERT(await_close(&f, fd, &s, 2000));

    close(fd);
    fixture_destroy(&f);
}

static void test_idle_connections_time_out(void)
{
    printf("\tTesting idle connections are closed...\n");

    fixture_t f;
    fixture_init(&f, 150, 0);
    int fd = client_open();
    stream_t s = {0};

    client_send(fd, GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(open_connections(&f) == 1);

    /* Quiet for longer than the timeout: the server closes it and lets go of it. */
    BB_ASSERT(await_close(&f, fd, &s, 2000));
    BB_ASSERT(open_connections(&f) == 0);

    close(fd);
    fixture_destroy(&f);
}

static void test_activity_postpones_the_idle_timeout(void)
{
    printf("\tTesting activity keeps a connection alive past the idle timeout...\n");

    fixture_t f;
    fixture_init(&f, 300, 0);
    int fd = client_open();
    stream_t s = {0};

    /* Six requests 100 ms apart: 600 ms in total, twice the timeout, but never
     * idle for more than 100 ms at a time. */
    for (size_t i = 1; i <= 6; i++)
    {
        client_send(fd, GET_PING);
        BB_ASSERT(await_responses(&f, fd, &s, i, false, 2000));
        run_for(&f, 100);
        stream_poll(&s, fd);
        BB_ASSERT(!s.eof);
    }

    /* Then quiet: now it goes. */
    BB_ASSERT(await_close(&f, fd, &s, 2000));

    close(fd);
    fixture_destroy(&f);
}

static void test_first_request_is_not_timed_out(void)
{
    printf("\tTesting a connection that has not sent a request yet is left alone...\n");

    fixture_t f;
    fixture_init(&f, 50, 0);
    int fd = client_open();
    stream_t s = {0};

    /* Documented: the idle timeout applies once a response has been written. */
    run_for(&f, 400);
    stream_poll(&s, fd);
    BB_ASSERT(!s.eof);

    client_send(fd, GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));

    close(fd);
    fixture_destroy(&f);
}

static void test_peer_hangup_releases_the_connection(void)
{
    printf("\tTesting a peer that hangs up releases its connection...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};

    client_send(fd, GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));
    BB_ASSERT(open_connections(&f) == 1);

    close(fd);

    int64_t deadline = bb_time_monotonic_ms() + 2000;
    while (open_connections(&f) != 0 && bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(f.runtime);
    }
    BB_ASSERT(open_connections(&f) == 0);

    fixture_destroy(&f);
}

static void test_websocket_upgrade_after_requests(void)
{
    printf("\tTesting a WebSocket upgrade on a connection that has served requests...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);
    int fd = client_open();
    stream_t s = {0};
    response_t r;

    client_send(fd, GET_PING);
    BB_ASSERT(await_responses(&f, fd, &s, 1, false, 2000));

    client_send(fd,
        "GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");

    /* The upgrade response has no body and, being 101, no Content-Length-framed
     * body either: wait for its header block. */
    int64_t deadline = bb_time_monotonic_ms() + 2000;
    while (!strstr(s.data + 0, "101") && bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(f.runtime);
        stream_poll(&s, fd);
    }

    BB_ASSERT(stream_response(&s, 1, false, &r));
    BB_ASSERT(r.status == 101);
    BB_ASSERT(has_header_value(&r, "Upgrade", "websocket"));
    BB_ASSERT(has_header_value(&r, "Connection", "Upgrade"));
    BB_ASSERT(find_header(&r, "Keep-Alive") == NULL);
    BB_ASSERT(!s.eof);

    close(fd);
    run_for(&f, 100);
    fixture_destroy(&f);
}

static void test_server_destroy_with_idle_connections(void)
{
    printf("\tTesting the server can be destroyed with idle persistent connections...\n");

    fixture_t f;
    fixture_init(&f, 5000, 0);

    int fds[3];
    stream_t streams[3];
    memset(streams, 0, sizeof(streams));

    for (int i = 0; i < 3; i++)
    {
        fds[i] = client_open();
        client_send(fds[i], GET_PING);
        BB_ASSERT(await_responses(&f, fds[i], &streams[i], 1, false, 2000));
    }
    BB_ASSERT(open_connections(&f) == 3);

    /* Leaks (connections, parsers, the sweep timer) are for LeakSanitizer. */
    fixture_destroy(&f);

    for (int i = 0; i < 3; i++)
    {
        stream_poll(&streams[i], fds[i]);
        BB_ASSERT(streams[i].eof);
        close(fds[i]);
    }
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    BB_ASSERT(bb_platform_net_init() == 0);

    printf("Running HTTP keep-alive tests...\n");

    test_two_requests_one_connection();
    test_connection_close_requested();
    test_http10();
    test_pipelined_requests_in_order();
    test_pipelined_request_arrives_in_pieces();
    test_request_bodies_keep_framing();
    test_head_response_has_no_body();
    test_no_body_statuses();
    test_handler_can_close();
    test_max_requests_per_connection();
    test_keepalive_can_be_disabled();
    test_idle_connections_time_out();
    test_activity_postpones_the_idle_timeout();
    test_first_request_is_not_timed_out();
    test_peer_hangup_releases_the_connection();
    test_websocket_upgrade_after_requests();
    test_server_destroy_with_idle_connections();

    printf("All HTTP keep-alive tests passed.\n");
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
