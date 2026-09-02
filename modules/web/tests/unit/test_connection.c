#include "connection/connection.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/error/error.h>
#include <blue-bird/utils/platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/select.h>
#endif

static void connection_create_test(void)
{
    printf("\tTesting connection creation...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    BB_ASSERT(conn != NULL);
    BB_ASSERT(conn->fd == fds[0]);
    BB_ASSERT(conn->state == BB_CONNECTION_READING);

    BB_ASSERT(conn->buffer != NULL);
    BB_ASSERT(conn->buffer_length == 0);
    BB_ASSERT(conn->buffer_capacity == 4096);

    BB_ASSERT(conn->write_data == NULL);
    BB_ASSERT(conn->write_pending == false);

    bb_connection_destroy(conn);
    bb_socket_close(fds[1]);
}

static void connection_buffer_add_test(void)
{
    printf("\tTesting write buffer enqueue...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    char *msg = malloc(6);
    memcpy(msg, "hello", 6);

    BB_ASSERT(bb_connection_buffer_add(conn, msg, 6) == 0);

    BB_ASSERT(conn->write_data != NULL);
    BB_ASSERT(conn->write_data->write_buffer == msg);
    BB_ASSERT(conn->write_data->write_length == 6);
    BB_ASSERT(conn->write_data->write_offset == 0);
    BB_ASSERT(conn->write_data->next == NULL);

    bb_connection_destroy(conn);
    bb_socket_close(fds[1]);
}

static void connection_multiple_buffers_test(void)
{
    printf("\tTesting multiple queued write buffers...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    char *a = malloc(2);
    char *b = malloc(2);
    char *c = malloc(2);

    strcpy(a, "A");
    strcpy(b, "B");
    strcpy(c, "C");

    BB_ASSERT(bb_connection_buffer_add(conn, a, 2) == 0);
    BB_ASSERT(bb_connection_buffer_add(conn, b, 2) == 0);
    BB_ASSERT(bb_connection_buffer_add(conn, c, 2) == 0);

    BB_ASSERT(conn->write_data == NULL ? 0 : 1);

    BB_ASSERT(conn->write_data->write_buffer == a);
    BB_ASSERT(conn->write_data->next->write_buffer == b);
    BB_ASSERT(conn->write_data->next->next->write_buffer == c);
    BB_ASSERT(conn->write_data->next->next->next == NULL);

    bb_connection_destroy(conn);
    bb_socket_close(fds[1]);
}

static void connection_zero_length_buffer_test(void)
{
    printf("\tTesting zero-length buffer...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    BB_ASSERT(bb_connection_buffer_add(conn, NULL, 0) == 0);
    BB_ASSERT(conn->write_data == NULL);

    bb_connection_destroy(conn);
    bb_socket_close(fds[1]);
}

static void connection_invalid_buffer_test(void)
{
    printf("\tTesting invalid buffer enqueue...\n");

    BB_ASSERT(bb_connection_buffer_add(NULL, NULL, 5) == -1);
}

static void connection_read_write_test(void)
{
    printf("\tTesting local socket read/write...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *reader = bb_connection_create_non_blocking(fds[0]);
    bb_connection_t *writer = bb_connection_create(fds[1]);

    char *msg = malloc(6);
    memcpy(msg, "hello", 6);

    BB_ASSERT(bb_connection_buffer_add(writer, msg, 6) == 0);
    BB_ASSERT(!BB_FAILED(bb_connection_write(writer)));

    
    BB_ASSERT(!BB_FAILED(bb_connection_read(reader)));
    BB_ASSERT(reader->buffer_length == 6);
    BB_ASSERT(memcmp(reader->buffer, "hello", 6) == 0);

    bb_connection_destroy(reader);
    bb_connection_destroy(writer);
}

static void connection_read_closed_test(void)
{
    printf("\tTesting read detects peer close...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *reader = bb_connection_create_non_blocking(fds[0]);

    bb_socket_close(fds[1]);

    bb_error_t err = bb_connection_read(reader);

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(reader->state == BB_CONNECTION_CLOSED);

    bb_connection_destroy(reader);
}

static void connection_write_closed_test(void)
{
    printf("\tTesting write detects peer close...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *writer = bb_connection_create(fds[0]);

    bb_socket_close(fds[1]);

    bb_error_t err = BB_SUCCESS();

    for (int i = 0; i < 1000; ++i)
    {
        char *msg = malloc(6);
        memcpy(msg, "hello", 6);

        BB_ASSERT(bb_connection_buffer_add(writer, msg, 6) == 0);

        err = bb_connection_write(writer);

        if (err.code == BB_ERR_CONNECTION_CLOSED)
            break;

        bb_usleep(1000);
    }

    BB_ASSERT(err.code == BB_ERR_CONNECTION_CLOSED);
    BB_ASSERT(writer->state == BB_CONNECTION_CLOSED);

    bb_connection_destroy(writer);
}

// ------------------------------------------------------------------------
// Buffer Management: hard bound on read-buffer growth (DoS protection)
//
// bb_connection_read() doubles its buffer as data streams in. Without a
// cap, a client that keeps sending bytes without ever completing a
// request (or many such clients at once) can force unbounded heap
// growth per connection -- a straightforward denial-of-service vector.
// These tests drive the buffer up to and past BB_CONNECTION_MAX_BUFFER_SIZE
// over a real socketpair and check the cap is enforced exactly, that no
// single growth step ever overshoots it, and that hitting it is treated
// as a normal, gracefully-reported error rather than a crash or hang.

// Sends up to `max_bytes` from `write_fd` into `reader`, draining via
// bb_connection_read() every time the kernel socket buffer would block
// on send(). This is essential for portability: blocking a full send()
// call before ever reading anything back deadlocks as soon as the
// chunk is bigger than the platform's default socket buffer (small on
// some platforms, e.g. under 64 KiB), since nothing would be left to
// drain it. Draining after every partial send means this never assumes
// a buffer size and can't hang. Stops early if bb_connection_read()
// reports an error. Writes the number of bytes actually sent to
// `out_sent` (may be less than `max_bytes` if it stopped early).
static bb_error_t _pump_bytes(bb_connection_t *reader, bb_socket_t write_fd, size_t max_bytes, size_t *out_sent)
{
    bb_socket_set_nonblocking(write_fd);

    char chunk[4096];
    memset(chunk, 'A', sizeof(chunk));

    size_t sent = 0;
    bb_error_t err = BB_SUCCESS();

    while (sent < max_bytes)
    {
        size_t remaining = max_bytes - sent;
        size_t to_send = remaining < sizeof(chunk) ? remaining : sizeof(chunk);

        ssize_t n = send(write_fd, chunk, to_send, 0);

        if (n > 0)
        {
            sent += (size_t)n;
        }
        else if (!(n < 0 && bb_socket_would_block()))
        {
            break; // unexpected send failure; nothing more we can do
        }

        // Drain whatever has arrived so far, whether or not this send
        // made progress -- this is what makes the loop above safe to
        // retry instead of spinning against a full kernel buffer.
        err = bb_connection_read(reader);
        if (BB_FAILED(err))
        {
            break;
        }
    }

    if (out_sent)
    {
        *out_sent = sent;
    }

    return err;
}

static void connection_read_buffer_capped_test(void)
{
    printf("\tTesting read buffer growth is capped under a flood of data...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *reader = bb_connection_create_non_blocking(fds[0]);

    // Send well past the cap (a slow/never-completing request is
    // exactly the shape this protects against) and confirm the read
    // eventually reports the payload as too large instead of growing
    // forever.
    size_t sent = 0;
    bb_error_t err = _pump_bytes(reader, fds[1], (size_t)BB_CONNECTION_MAX_BUFFER_SIZE * 2, &sent);

    BB_ASSERT(err.code == BB_ERR_PAYLOAD_TOO_LARGE);
    BB_ASSERT(reader->state == BB_CONNECTION_CLOSED);

    // The cap must be a hard ceiling: no growth step is allowed to
    // overshoot it, and it must actually have been reached (not just
    // failed some other way early).
    BB_ASSERT(reader->buffer_capacity == BB_CONNECTION_MAX_BUFFER_SIZE);
    BB_ASSERT(reader->buffer_length < reader->buffer_capacity);

    bb_connection_destroy(reader);
    bb_socket_close(fds[1]);
}

static void connection_closed_is_sticky_test(void)
{
    printf("\tTesting closed state is sticky...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    conn->state = BB_CONNECTION_CLOSED;

    BB_ASSERT(bb_connection_read(conn).code == BB_ERR_CONNECTION_CLOSED);

    BB_ASSERT(bb_connection_write(conn).code == BB_ERR_CONNECTION_CLOSED);

    bb_connection_destroy(conn);
    bb_socket_close(fds[1]);
}

int main(void)
{
    bb_platform_net_init(); // Ignore SIGPIPE
    printf("Running connection unit tests...\n");

    connection_create_test();
    connection_buffer_add_test();
    connection_multiple_buffers_test();
    connection_zero_length_buffer_test();
    connection_invalid_buffer_test();
    connection_read_write_test();
    connection_read_buffer_capped_test();
    connection_read_closed_test();
    connection_write_closed_test();
    connection_closed_is_sticky_test();

    printf("All connection tests passed.\n");

    return 0;
}
