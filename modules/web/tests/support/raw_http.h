#ifndef BB_TEST_RAW_HTTP_H
#define BB_TEST_RAW_HTTP_H

/*
 * A plain non-blocking TCP client for the single-threaded server tests, and a
 * reader that picks responses off the stream by their framing (status line,
 * headers, Content-Length) rather than by waiting for the connection to close.
 * POSIX only; the tests that include it are skipped on Windows.
 */

#include "blue-bird/runtime/runtime.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/time.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

static inline int raw_client_open(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(fd >= 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    BB_ASSERT(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    BB_ASSERT(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    BB_ASSERT(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);

    return fd;
}

static inline void client_send(int fd, const char *data)
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
static inline void stream_poll(stream_t *s, int fd)
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

static inline const char *find_header(const response_t *r, const char *name)
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

static inline bool has_header_value(const response_t *r, const char *name, const char *prefix)
{
    const char *v = find_header(r, name);
    return v && strncasecmp(v, prefix, strlen(prefix)) == 0;
}

/* Parses the index-th response in the stream by its framing. `head` marks a
 * response to HEAD, whose Content-Length describes a body that is not sent.
 * Returns false while that response is still incomplete. */
static inline bool stream_response(const stream_t *s, size_t index, bool head, response_t *out)
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

static inline bool body_is(const response_t *r, const char *expected)
{
    return r->body_len == strlen(expected) && memcmp(r->body, expected, r->body_len) == 0;
}

/* Drives the server until the stream holds `count` complete responses. */
static inline bool await_responses(bb_runtime_t *runtime, int fd, stream_t *s, size_t count, bool head, int timeout_ms)
{
    int64_t deadline = bb_time_monotonic_ms() + timeout_ms;
    response_t r;

    for (;;)
    {
        bb_runtime_tick(runtime);
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
static inline bool await_close(bb_runtime_t *runtime, int fd, stream_t *s, int ms)
{
    int64_t deadline = bb_time_monotonic_ms() + ms;

    while (bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(runtime);
        stream_poll(s, fd);
        if (s->eof)
        {
            return true;
        }
    }
    return false;
}

/* Drives the server for `ms` without anything else happening. */
static inline void run_for(bb_runtime_t *runtime, int ms)
{
    int64_t deadline = bb_time_monotonic_ms() + ms;
    while (bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(runtime);
    }
}

#define GET_PING "GET /ping HTTP/1.1\r\nHost: x\r\n\r\n"
#define GET_ID(n) "GET /id/" #n " HTTP/1.1\r\nHost: x\r\n\r\n"

#endif /* BB_TEST_RAW_HTTP_H */
