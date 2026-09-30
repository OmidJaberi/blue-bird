#include "server_internal.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <blue-bird/error/assert.h>

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

static void http_get(const char *path)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(18099) };
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    BB_ASSERT(connect(fd, (struct sockaddr *) &a, sizeof(a)) == 0);
    char req[256];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", path);
    BB_ASSERT(write(fd, req, n) == n);
    char buf[1024];
    while (read(fd, buf, sizeof(buf)) > 0) {}
    close(fd);
}

int main(void)
{
    rt = bb_runtime_create();
    bb_server_t *server = bb_server_create_on_runtime(rt, 18099);
    BB_ASSERT(server);
    bb_server_add_route(server, "GET", "/ok", ok_handler);
    bb_server_start(server);

    pthread_t th;
    pthread_create(&th, NULL, server_thread, NULL);
    while (!bb_runtime_is_running(rt)) usleep(1000);

    for (int i = 0; i < 5; i++) http_get("/ok");
    for (int i = 0; i < 2; i++) http_get("/nope");
    usleep(200000);

    bb_runtime_stop(rt);
    pthread_join(th, NULL);

    const bb_metrics_t *m = &server->metrics;
    printf("accepted=%llu requests=%llu 2xx=%llu 4xx=%llu latency_count=%llu sum_ms=%llu\n",
           (unsigned long long) m->connections_accepted_total,
           (unsigned long long) m->requests_total,
           (unsigned long long) m->responses_by_class[2],
           (unsigned long long) m->responses_by_class[4],
           (unsigned long long) m->latency_count,
           (unsigned long long) m->latency_sum_ms);
    printf("active_conns=%zu active_ws=%zu\n", bb_conn_list_count(server->conn_list), bb_ws_list_count(server->ws_list));

    BB_ASSERT(m->connections_accepted_total == 7);
    BB_ASSERT(m->requests_total == 7);
    BB_ASSERT(m->responses_by_class[2] == 5);
    BB_ASSERT(m->responses_by_class[4] == 2);
    BB_ASSERT(m->latency_count == 7);
    BB_ASSERT(bb_conn_list_count(server->conn_list) == 0);
    BB_ASSERT(bb_ws_list_count(server->ws_list) == 0);

    bb_server_destroy(server);
    bb_runtime_destroy(rt);
    printf("All metrics pipeline tests passed.\n");
    return 0;
}
