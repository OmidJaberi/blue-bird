#include "connection/connection.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void connection_create_test(void)
{
    printf("\tTesting connection creation...\n");

    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    assert(conn != NULL);
    assert(conn->fd == fds[0]);
    assert(conn->state == BB_CONNECTION_READING);

    assert(conn->buffer != NULL);
    assert(conn->buffer_length == 0);
    assert(conn->buffer_capacity == 4096);

    assert(conn->write_data == NULL);
    assert(conn->write_pending == false);

    bb_connection_destroy(conn);
    close(fds[1]);
}

static void connection_buffer_add_test(void)
{
    printf("\tTesting write buffer enqueue...\n");

    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    char *msg = malloc(6);
    memcpy(msg, "hello", 6);

    assert(bb_connection_buffer_add(conn, msg, 6) == 0);

    assert(conn->write_data != NULL);
    assert(conn->write_data->write_buffer == msg);
    assert(conn->write_data->write_length == 6);
    assert(conn->write_data->write_offset == 0);
    assert(conn->write_data->next == NULL);

    bb_connection_destroy(conn);
    close(fds[1]);
}

static void connection_multiple_buffers_test(void)
{
    printf("\tTesting multiple queued write buffers...\n");

    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    char *a = malloc(2);
    char *b = malloc(2);
    char *c = malloc(2);

    strcpy(a, "A");
    strcpy(b, "B");
    strcpy(c, "C");

    assert(bb_connection_buffer_add(conn, a, 2) == 0);
    assert(bb_connection_buffer_add(conn, b, 2) == 0);
    assert(bb_connection_buffer_add(conn, c, 2) == 0);

    assert(conn->write_data == NULL ? 0 : 1);

    assert(conn->write_data->write_buffer == a);
    assert(conn->write_data->next->write_buffer == b);
    assert(conn->write_data->next->next->write_buffer == c);
    assert(conn->write_data->next->next->next == NULL);

    bb_connection_destroy(conn);
    close(fds[1]);
}

static void connection_zero_length_buffer_test(void)
{
    printf("\tTesting zero-length buffer...\n");

    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *conn = bb_connection_create(fds[0]);

    assert(bb_connection_buffer_add(conn, NULL, 0) == 0);
    assert(conn->write_data == NULL);

    bb_connection_destroy(conn);
    close(fds[1]);
}

static void connection_invalid_buffer_test(void)
{
    printf("\tTesting invalid buffer enqueue...\n");

    assert(bb_connection_buffer_add(NULL, NULL, 5) == -1);
}

static void connection_read_write_test(void)
{
    printf("\tTesting local socket read/write...\n");

    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_connection_t *reader = bb_connection_create(fds[0]);
    bb_connection_t *writer = bb_connection_create(fds[1]);

    char *msg = malloc(6);
    memcpy(msg, "hello", 6);

    assert(bb_connection_buffer_add(writer, msg, 6) == 0);
    assert(bb_connection_write(writer) == 1);

    ssize_t n = bb_connection_read(reader);

    assert(n == 6);
    assert(reader->buffer_length == 6);
    assert(memcmp(reader->buffer, "hello", 6) == 0);

    bb_connection_destroy(reader);
    bb_connection_destroy(writer);
}

int main(void)
{
    printf("Running connection unit tests...\n");

    connection_create_test();
    connection_buffer_add_test();
    connection_multiple_buffers_test();
    connection_zero_length_buffer_test();
    connection_invalid_buffer_test();
    connection_read_write_test();

    printf("All connection tests passed.\n");

    return 0;
}
