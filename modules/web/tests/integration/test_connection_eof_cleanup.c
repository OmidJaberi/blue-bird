#include "blue-bird/web/server.h"
#include "blue-bird/utils/platform.h" /* portable sockets, bb_usleep(); pulls in winsock2 on Windows */

#include <blue-bird/error/assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/*
 * Regression test: a client that closes before a response is written must not
 * leave its connection behind (fd, connection-list entry, task data). The leak
 * showed up as bluebird_http_connections_active (and the server's open fds)
 * growing by one per connect-and-close.
 */

#define TEST_PORT 18101
#define ROUNDS 25

static bb_runtime_t *rt;

static bb_error_t ok_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "hi");
    return BB_SUCCESS();
}

static void *server_thread(void *arg)
{
    (void) arg;
    bb_runtime_run(rt);
    return NULL;
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
    return fd;
}

static void send_all(bb_socket_t fd, const char *s)
{
    int len = (int) strlen(s);
    BB_ASSERT(send(fd, s, len, MSG_NOSIGNAL) == len);
}

int main(void)
{
    rt = bb_runtime_create();
    bb_server_t *server = bb_server_create_on_runtime(rt, TEST_PORT);
    BB_ASSERT(server != NULL);
    bb_server_add_route(server, "GET", "/ok", ok_handler);
    bb_server_start(server);

    pthread_t th;
    pthread_create(&th, NULL, server_thread, NULL);
    while (!bb_runtime_is_running(rt)) bb_usleep(1000);

    printf("Testing connections closed before / during a request are cleaned up...\n");
    for (int i = 0; i < ROUNDS; i++)
    {
        /* connect, close without sending anything */
        bb_socket_t a = client_connect();
        bb_socket_close(a);

        /* send a truncated request line, close */
        bb_socket_t b = client_connect();
        send_all(b, "GET /ok HTT");
        bb_socket_close(b);

        /* complete request, close without reading the response */
        bb_socket_t c = client_connect();
        send_all(c, "GET /ok HTTP/1.1\r\nHost: x\r\n\r\n");
        bb_socket_close(c);
    }

    bb_usleep(500000); /* let the loop observe every close */

    bb_runtime_stop(rt);
    pthread_join(th, NULL);

    bb_metrics_snapshot_t snap;
    BB_ASSERT(bb_server_get_metrics(server, &snap) == 0);
    printf("accepted=%llu active_connections=%llu\n",
           (unsigned long long) snap.counters.connections_accepted_total,
           (unsigned long long) snap.active_connections);

    BB_ASSERT(snap.counters.connections_accepted_total == 3 * ROUNDS);
    BB_ASSERT(snap.active_connections == 0);
    BB_ASSERT(snap.active_ws_sessions == 0);

    bb_server_destroy(server);
    bb_runtime_destroy(rt);

    printf("All connection EOF cleanup tests passed.\n");
    return 0;
}
