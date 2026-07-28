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
    connection_read_closed_test();
    connection_write_closed_test();
    connection_closed_is_sticky_test();

    printf("All connection tests passed.\n");

    return 0;
}
