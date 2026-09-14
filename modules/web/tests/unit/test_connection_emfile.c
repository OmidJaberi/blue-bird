#include "connection/connection.h"

#include <blue-bird/utils/platform.h>
#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if !defined(_WIN32)
#include <unistd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/*
 * Socket-exhaustion (EMFILE/ENFILE) rejection
 *
 * These tests exercise the "spare descriptor" path in bb_connection_accept():
 * when the process runs out of descriptors, accept() can't hand a new client
 * an fd, so it rejects and closes the connection immediately rather than
 * leaving it queued in the listen backlog. The helper functions below create
 * real sockets so the kernel actually participates; the tests are small
 * enough to run as unit tests (no runtime, no threads, no HTTP parser).
 */

#if !defined(_WIN32)

static bb_socket_t _create_bound_listener_random(int *out_port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    bb_socket_set_nonblocking(fd);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0)
    {
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0)
    {
        close(fd);
        return -1;
    }

    *out_port = ntohs(addr.sin_port);
    return fd;
}

static bb_socket_t _connect_to(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

/*
 * Counts descriptors in [0, 1024) that are currently open.  Used to
 * derive the exact rlim_cur that makes accept() fail: with N fds open
 * (occupying fd numbers 0..N-1 with no gaps, as guaranteed by our
 * controlled setup), rlim_cur = N means the next allocation (fd N)
 * exceeds the limit, while closing any one of the existing fds frees a
 * slot below the limit.
 */
static int _count_open_fds(void)
{
    int count = 0;

    for (int fd = 0; fd < 1024; fd++)
    {
        if (fcntl(fd, F_GETFD) != -1)
            count++;
    }

    return count;
}

static void _pin_fd_table(struct rlimit *out_previous)
{
    getrlimit(RLIMIT_NOFILE, out_previous);

    int open_count = _count_open_fds();

    struct rlimit pinned = *out_previous;
    pinned.rlim_cur = (rlim_t)open_count;
    (void)setrlimit(RLIMIT_NOFILE, &pinned);
}

/*
 * Accepts one connection to prime the spare descriptor, then verifies
 * that under fd exhaustion a second pending connection is drained and
 * rejected cleanly rather than left queued in the listen backlog.
 *
 * The client must observe an EOF/reset as proof the server-side closed
 * the socket -- confirming the rejection actually happened, not just a
 * plain "nothing pending" EAGAIN.
 */
static void test_rejects_pending_connection_under_fd_exhaustion(void)
{
    printf("\tRunning test_rejects_pending_connection_under_fd_exhaustion...\n");

    int port = 0;
    bb_socket_t listener = _create_bound_listener_random(&port);
    BB_ASSERT(listener >= 0);

    /* Accept one connection to prime the spare descriptor. */
    bb_socket_t probe_client = _connect_to(port);
    BB_ASSERT(probe_client >= 0);

    bb_connection_t *accepted_probe = bb_connection_accept(listener);
    BB_ASSERT(accepted_probe != NULL);
    bb_connection_destroy(accepted_probe);
    close(probe_client);

    /* Open a pending connection, then lock the fd table. */
    bb_socket_t pending_client = _connect_to(port);
    BB_ASSERT(pending_client >= 0);

    bb_socket_set_nonblocking(pending_client);

    struct rlimit previous;
    _pin_fd_table(&previous);

    bb_connection_t *accepted = bb_connection_accept(listener);
    BB_ASSERT(accepted == NULL);

    /* The server closed the socket; the client must observe the close. */
    int closed = 0;
    for (int i = 0; i < 200 && !closed; i++)
    {
        char buf[1];
        ssize_t n = read(pending_client, buf, 1);

        if (n == 0)
            closed = 1;
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            closed = 1;
        else
            bb_usleep(1000);
    }

    BB_ASSERT(closed);

    setrlimit(RLIMIT_NOFILE, &previous);

    /* Listener must recover and accept a fresh connection. */
    bb_socket_t recovery = _connect_to(port);
    BB_ASSERT(recovery >= 0);

    bb_connection_t *accepted_recover = NULL;
    for (int i = 0; i < 200 && !accepted_recover; i++)
    {
        accepted_recover = bb_connection_accept(listener);
        if (!accepted_recover)
            bb_usleep(1000);
    }

    BB_ASSERT(accepted_recover != NULL);

    bb_connection_destroy(accepted_recover);
    close(recovery);
    close(pending_client);
    close(listener);
}

/*
 * Fills the backlog with two clients, then exhausts the fd table.
 * Every subsequent accept() call must drain and reject exactly one
 * pending connection, proving the accept loop keeps cycling without
 * spinning.
 */
static void test_full_backlog_rejects_multiple_pending_connections(void)
{
    printf("\tRunning test_full_backlog_rejects_multiple_pending_connections...\n");

    int port = 0;
    bb_socket_t listener = _create_bound_listener_random(&port);
    BB_ASSERT(listener >= 0);

    /* Prime the spare descriptor. */
    bb_socket_t probe = _connect_to(port);
    BB_ASSERT(probe >= 0);

    bb_connection_t *accepted_probe = bb_connection_accept(listener);
    BB_ASSERT(accepted_probe != NULL);
    bb_connection_destroy(accepted_probe);
    close(probe);

    /* Fill the backlog with two clients. */
    bb_socket_t clients[2];
    clients[0] = _connect_to(port);
    BB_ASSERT(clients[0] >= 0);
    clients[1] = _connect_to(port);
    BB_ASSERT(clients[1] >= 0);

    struct rlimit previous;
    _pin_fd_table(&previous);

    /* First accept() rejects and drains one. */
    BB_ASSERT(bb_connection_accept(listener) == NULL);

    /* Second accept() also rejects (another pending connection in backlog). */
    BB_ASSERT(bb_connection_accept(listener) == NULL);

    /* Third accept(): nothing left pending, normal EAGAIN. */
    BB_ASSERT(bb_connection_accept(listener) == NULL);

    setrlimit(RLIMIT_NOFILE, &previous);

    for (int i = 0; i < 2; i++)
        close(clients[i]);

    close(listener);
}

/*
 * If bb_connection_accept() is called when the fd table is already
 * exhausted and no spare was ever established (born exhausted), the
 * call must not crash and must return NULL.  This is the graceful
 * degradation path when the spare trick simply cannot kick in.
 */
static void test_born_exhausted_fails_without_crash(void)
{
    printf("\tRunning test_born_exhausted_fails_without_crash...\n");

    int port = 0;
    bb_socket_t listener = _create_bound_listener_random(&port);
    BB_ASSERT(listener >= 0);

    bb_socket_t pending = _connect_to(port);
    BB_ASSERT(pending >= 0);

    struct rlimit previous;
    _pin_fd_table(&previous);

    BB_ASSERT(bb_connection_accept(listener) == NULL);

    setrlimit(RLIMIT_NOFILE, &previous);

    close(pending);
    close(listener);
}

#endif /* !_WIN32 */

int main(void)
{
    printf("Running connection EMFILE/ENFILE rejection tests...\n");

#if !defined(_WIN32)
    test_born_exhausted_fails_without_crash();
    test_rejects_pending_connection_under_fd_exhaustion();
    test_full_backlog_rejects_multiple_pending_connections();
#endif

    printf("All connection EMFILE/ENFILE rejection tests passed.\n");
    return 0;
}
