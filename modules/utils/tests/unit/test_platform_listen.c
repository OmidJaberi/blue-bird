#include <stdio.h>
#include <string.h>
#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>

static bb_socket_t _bind_loopback(void)
{
    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(!bb_socket_is_invalid(fd));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; /* ephemeral */
    BB_ASSERT(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    return fd;
}

static void test_backlog_default_is_generous(void)
{
    printf("Testing listen backlog default...\n");
    /* The old hard-coded value of 3 starved bursty accept queues. */
    BB_ASSERT(BB_LISTEN_BACKLOG >= 128);
}

static void test_listen_with_default_backlog(void)
{
    printf("Testing bb_socket_listen with default backlog...\n");
    bb_socket_t fd = _bind_loopback();
    BB_ASSERT(bb_socket_listen(fd, 0) == 0);
    bb_socket_close(fd);

    fd = _bind_loopback();
    BB_ASSERT(bb_socket_listen(fd, -5) == 0);
    bb_socket_close(fd);

    fd = _bind_loopback();
    BB_ASSERT(bb_socket_listen(fd, BB_LISTEN_BACKLOG) == 0);
    bb_socket_close(fd);
}

static void test_listen_with_explicit_backlog(void)
{
    printf("Testing bb_socket_listen with explicit backlog...\n");
    bb_socket_t fd = _bind_loopback();
    BB_ASSERT(bb_socket_listen(fd, 1) == 0);
    bb_socket_close(fd);
}

static void test_listen_on_invalid_socket_fails(void)
{
    printf("Testing bb_socket_listen on invalid socket...\n");
    BB_ASSERT(bb_socket_listen(BB_INVALID_SOCKET, 0) == -1);
}

int main(void)
{
    BB_ASSERT(bb_platform_net_init() == 0);
    test_backlog_default_is_generous();
    test_listen_with_default_backlog();
    test_listen_with_explicit_backlog();
    test_listen_on_invalid_socket_fails();
    bb_platform_net_cleanup();
    printf("All platform listen tests passed.\n");
    return 0;
}
