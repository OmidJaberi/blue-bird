/*
 * bb_connection_discard_read(): dropping a handled request from the read
 * buffer while keeping any bytes behind it (a pipelined next request).
 */
#if !defined(_WIN32)

#include "connection/connection.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct
{
    int peer;
    bb_connection_t *conn;
} fixture_t;

static void fixture_init(fixture_t *f)
{
    int pair[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    f->peer = pair[1];
    f->conn = bb_connection_create(pair[0]);
    BB_ASSERT(f->conn != NULL);
}

static void fixture_destroy(fixture_t *f)
{
    bb_connection_destroy(f->conn); /* closes its end */
    close(f->peer);
}

static void fill(bb_connection_t *c, const char *data)
{
    size_t len = strlen(data);
    BB_ASSERT(len + 1 < c->buffer_capacity);
    memcpy(c->buffer, data, len + 1);
    c->buffer_length = len;
}

static void test_discards_the_front_and_keeps_the_rest(void)
{
    printf("\tTesting discard keeps the bytes behind the dropped ones...\n");

    fixture_t f;
    fixture_init(&f);
    fill(f.conn, "GET /a HTTP/1.1\r\n\r\nGET /b HT");

    bb_connection_discard_read(f.conn, strlen("GET /a HTTP/1.1\r\n\r\n"));

    BB_ASSERT(f.conn->buffer_length == strlen("GET /b HT"));
    BB_ASSERT(strcmp(f.conn->buffer, "GET /b HT") == 0); /* still NUL-terminated */

    fixture_destroy(&f);
}

static void test_discard_everything_and_edge_cases(void)
{
    printf("\tTesting discard of everything, nothing and too much...\n");

    fixture_t f;
    fixture_init(&f);

    fill(f.conn, "abcdef");
    bb_connection_discard_read(f.conn, 0);
    BB_ASSERT(f.conn->buffer_length == 6 && strcmp(f.conn->buffer, "abcdef") == 0);

    bb_connection_discard_read(f.conn, 6);
    BB_ASSERT(f.conn->buffer_length == 0 && f.conn->buffer[0] == '\0');

    /* More than is there clamps instead of reading past the end. */
    fill(f.conn, "abc");
    bb_connection_discard_read(f.conn, 1000);
    BB_ASSERT(f.conn->buffer_length == 0 && f.conn->buffer[0] == '\0');

    /* Empty buffer, and no connection at all. */
    bb_connection_discard_read(f.conn, 5);
    BB_ASSERT(f.conn->buffer_length == 0);
    bb_connection_discard_read(NULL, 5);

    fixture_destroy(&f);
}

static void test_small_buffer_is_kept(void)
{
    printf("\tTesting an ordinary buffer is not reallocated...\n");

    fixture_t f;
    fixture_init(&f);

    size_t capacity = f.conn->buffer_capacity;
    char *buffer = f.conn->buffer;

    fill(f.conn, "abc");
    bb_connection_discard_read(f.conn, 3);

    BB_ASSERT(f.conn->buffer_capacity == capacity);
    BB_ASSERT(f.conn->buffer == buffer);

    fixture_destroy(&f);
}

/* A big request grows the buffer; once it has been handled an idle persistent
 * connection must not go on holding that memory. */
static void test_emptied_large_buffer_is_released(void)
{
    printf("\tTesting an emptied large buffer is handed back...\n");

    fixture_t f;
    fixture_init(&f);

    size_t initial = f.conn->buffer_capacity;
    size_t big = 256 * 1024;
    char *grown = realloc(f.conn->buffer, big);
    BB_ASSERT(grown != NULL);
    f.conn->buffer = grown;
    f.conn->buffer_capacity = big;

    memset(f.conn->buffer, 'x', 1000);
    f.conn->buffer[1000] = '\0';
    f.conn->buffer_length = 1000;

    /* Not empty yet (the start of the next request is still there): keep it. */
    bb_connection_discard_read(f.conn, 600);
    BB_ASSERT(f.conn->buffer_length == 400);
    BB_ASSERT(f.conn->buffer_capacity == big);

    /* Emptied: back to the initial size, and still a valid empty string. */
    bb_connection_discard_read(f.conn, 400);
    BB_ASSERT(f.conn->buffer_length == 0);
    BB_ASSERT(f.conn->buffer_capacity == initial);
    BB_ASSERT(f.conn->buffer[0] == '\0');

    fixture_destroy(&f);
}

int main(void)
{
    printf("Running connection discard tests...\n");

    test_discards_the_front_and_keeps_the_rest();
    test_discard_everything_and_edge_cases();
    test_small_buffer_is_kept();
    test_emptied_large_buffer_is_released();

    printf("All connection discard tests passed.\n");
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
