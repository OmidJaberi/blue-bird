#include "transport/transport.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>

#include <stdio.h>
#include <string.h>

static void transport_tcp_create_test(void)
{
    printf("\tTesting TCP transport creation...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_transport_t *transport = bb_transport_create_tcp(fds[0]);
    BB_ASSERT(transport != NULL);

    bb_transport_destroy(transport);
    bb_socket_close(fds[0]);
    bb_socket_close(fds[1]);
}

static void transport_tcp_read_write_test(void)
{
    printf("\tTesting TCP transport read/write...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_transport_t *a = bb_transport_create_tcp(fds[0]);
    bb_transport_t *b = bb_transport_create_tcp(fds[1]);

    size_t written = 0;
    bb_transport_status_t st = bb_transport_write(a, "hello", 5, &written);
    BB_ASSERT(st == BB_TRANSPORT_OK);
    BB_ASSERT(written == 5);

    char buf[16] = {0};
    size_t read_n = 0;
    st = bb_transport_read(b, buf, sizeof(buf), &read_n);
    BB_ASSERT(st == BB_TRANSPORT_OK);
    BB_ASSERT(read_n == 5);
    BB_ASSERT(memcmp(buf, "hello", 5) == 0);

    bb_transport_destroy(a);
    bb_transport_destroy(b);
    bb_socket_close(fds[0]);
    bb_socket_close(fds[1]);
}

static void transport_tcp_want_read_test(void)
{
    printf("\tTesting TCP transport WANT_READ on empty non-blocking socket...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    BB_ASSERT(bb_socket_set_nonblocking(fds[0]) == 0);

    bb_transport_t *a = bb_transport_create_tcp(fds[0]);

    char buf[16];
    size_t n = 0;
    bb_transport_status_t st = bb_transport_read(a, buf, sizeof(buf), &n);
    BB_ASSERT(st == BB_TRANSPORT_WANT_READ);

    bb_transport_destroy(a);
    bb_socket_close(fds[0]);
    bb_socket_close(fds[1]);
}

static void transport_tcp_closed_test(void)
{
    printf("\tTesting TCP transport CLOSED on peer shutdown...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_socket_close(fds[1]); /* peer goes away */

    bb_transport_t *a = bb_transport_create_tcp(fds[0]);

    char buf[16];
    size_t n = 0;
    bb_transport_status_t st = bb_transport_read(a, buf, sizeof(buf), &n);
    BB_ASSERT(st == BB_TRANSPORT_CLOSED);

    bb_transport_destroy(a);
    bb_socket_close(fds[0]);
}

/* Portable "is this still an open socket handle" check -- fcntl(F_GETFD)
 * is POSIX-only and doesn't exist on Windows, but SOL_SOCKET/SO_TYPE is
 * supported by both Winsock and BSD sockets. */
static int _socket_is_open(bb_socket_t sock)
{
    int type = 0;
    socklen_t len = sizeof(type);
    return getsockopt(sock, SOL_SOCKET, SO_TYPE, (char *)&type, &len) == 0;
}

/* destroy() must never close the underlying fd -- bb_connection_t owns
 * socket lifetime independently of whichever transport is layered over
 * it (this matters for TLS upgrade in place). Verify the fd is still a
 * valid, open socket after the transport is torn down. */
static void transport_tcp_destroy_does_not_close_fd_test(void)
{
    printf("\tTesting TCP transport destroy leaves fd open...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_transport_t *a = bb_transport_create_tcp(fds[0]);
    bb_transport_destroy(a);

    BB_ASSERT(_socket_is_open(fds[0]));

    bb_socket_close(fds[0]);
    bb_socket_close(fds[1]);
}

static void transport_dispatch_null_safety_test(void)
{
    printf("\tTesting transport dispatch NULL safety...\n");

    BB_ASSERT(bb_transport_handshake(NULL) == BB_TRANSPORT_ERROR);
    BB_ASSERT(bb_transport_shutdown(NULL) == BB_TRANSPORT_OK);

    size_t n = 0;
    char buf[4];
    BB_ASSERT(bb_transport_read(NULL, buf, sizeof(buf), &n) == BB_TRANSPORT_ERROR);
    BB_ASSERT(bb_transport_write(NULL, buf, sizeof(buf), &n) == BB_TRANSPORT_ERROR);

    bb_transport_destroy(NULL); /* must not crash */
}

/* Plain TCP has no handshake or shutdown negotiation to do -- both
 * optional ops are NULL and must report success immediately, exactly
 * matching pre-transport-layer behavior of "TCP just works". */
static void transport_tcp_optional_ops_test(void)
{
    printf("\tTesting TCP transport optional ops (handshake/shutdown)...\n");

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    bb_transport_t *a = bb_transport_create_tcp(fds[0]);

    BB_ASSERT(bb_transport_handshake(a) == BB_TRANSPORT_OK);
    BB_ASSERT(bb_transport_shutdown(a) == BB_TRANSPORT_OK);

    bb_transport_destroy(a);
    bb_socket_close(fds[0]);
    bb_socket_close(fds[1]);
}

int main(void)
{
    printf("Testing transport...\n");

    bb_platform_net_init();

    transport_tcp_create_test();
    transport_tcp_read_write_test();
    transport_tcp_want_read_test();
    transport_tcp_closed_test();
    transport_tcp_destroy_does_not_close_fd_test();
    transport_dispatch_null_safety_test();
    transport_tcp_optional_ops_test();

    bb_platform_net_cleanup();

    printf("Transport tests passed.\n");
    return 0;
}
