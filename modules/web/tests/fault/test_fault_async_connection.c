/*
 * Failure injection for the async connection layer: creating, serving,
 * accepting and connecting connections under allocation failures, and
 * scheduling read and write tasks under runtime registration failures.
 *
 * A failed operation must report the failure, leave the connection exactly as
 * it was (so the same call can simply be retried), and release everything it
 * took, including descriptors: a leaked listening socket would keep the port
 * bound. See fault/fault_inject.h for how failures are injected.
 */
#include "fault_inject.h"
#include "fault_util.h"

#include "connection/async_connection.h"
#include "blue-bird/runtime/runtime.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TEST_PORT 19474
#define TEST_PORT_STR "19474"

#define MAX_SWEEP 1000

/* ============================================================
 * Helpers
 * ============================================================ */

static void dummy_write_done(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

static bb_read_status_t dummy_read_step(void *userdata)
{
    (void)userdata;
    return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
}

static void dummy_read_error(bb_error_t err, void *userdata)
{
    (void)err;
    (void)userdata;
}

/* A plain client socket connected to the test listener (completes against the
 * listen backlog, so no accept is needed on the other side). */
static int raw_client(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(fd >= 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    BB_ASSERT(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    BB_ASSERT(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    return fd;
}

/* Accepts one pending client with nothing armed. */
static bb_async_connection_t *accept_one(bb_runtime_t *runtime, bb_async_connection_t *listener)
{
    /* The kernel may need a moment to complete the handshake of a connection
     * that was just made; the listener is non-blocking. */
    for (int i = 0; i < 1000; i++)
    {
        bb_async_connection_t *conn = bb_async_connection_accept(runtime, listener->connection->fd);
        if (conn)
        {
            return conn;
        }
        bb_usleep(1000);
    }
    return NULL;
}

/* The first accept opens the process-lifetime spare descriptor; take it out of
 * the descriptor accounting. */
static void warm_up(void)
{
    bb_fi_reset();

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime);
    bb_async_connection_t *listener = bb_async_connection_serve(runtime, TEST_PORT);
    BB_ASSERT(listener);

    int client = raw_client();
    bb_async_connection_t *conn = accept_one(runtime, listener);
    BB_ASSERT(conn);

    bb_async_connection_destroy(conn);
    close(client);
    bb_async_connection_destroy(listener);
    bb_runtime_destroy(runtime);
}

/* ============================================================
 * Creation, serve, accept, connect: allocation failures
 * ============================================================ */

static void test_create_allocation_sweep(void)
{
    printf("\tTesting bb_async_connection_create() under allocation failures...\n");

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime);

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        bb_fi_fail_alloc_at(n);
        bb_async_connection_t *conn = bb_async_connection_create(runtime);
        bool injected = bb_fi_alloc_failures() > 0;
        bb_fi_reset();

        if (!injected)
        {
            fault_check(conn != NULL, "creation succeeds when nothing fails", n);
            bb_async_connection_destroy(conn);
            break;
        }

        failures_swept++;
        fault_check(conn == NULL, "a failed allocation yields NULL", n);
    }

    BB_ASSERT(failures_swept >= 1);
    bb_runtime_destroy(runtime);
}

static void test_serve_allocation_sweep(void)
{
    printf("\tTesting bb_async_connection_serve() under allocation failures...\n");

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        int fds_before = fault_count_open_fds();

        bb_runtime_t *runtime = bb_runtime_create();
        BB_ASSERT(runtime);

        bb_fi_fail_alloc_at(n);
        bb_async_connection_t *listener = bb_async_connection_serve(runtime, TEST_PORT);
        bool injected = bb_fi_alloc_failures() > 0;
        bb_fi_reset();

        if (!injected)
        {
            fault_check(listener != NULL, "serve succeeds when nothing fails", n);
            bb_async_connection_destroy(listener);
            bb_runtime_destroy(runtime);
            fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
            break;
        }

        failures_swept++;
        fault_check(listener == NULL, "a failed allocation yields NULL", n);

        /* A leaked listening socket would keep the port bound. */
        bb_async_connection_t *again = bb_async_connection_serve(runtime, TEST_PORT);
        fault_check(again != NULL, "the port is free again after the failure", n);
        bb_async_connection_destroy(again);

        bb_runtime_destroy(runtime);
        fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
    }

    BB_ASSERT(failures_swept >= 2);
    printf("\t  (%u allocation points failed in turn)\n", failures_swept);
}

static void test_accept_allocation_sweep(void)
{
    printf("\tTesting bb_async_connection_accept() under allocation failures...\n");

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        int fds_before = fault_count_open_fds();

        bb_runtime_t *runtime = bb_runtime_create();
        BB_ASSERT(runtime);
        bb_async_connection_t *listener = bb_async_connection_serve(runtime, TEST_PORT);
        BB_ASSERT(listener);

        int client = raw_client();

        /* Let the connection finish queueing before arming, so the failure
         * lands inside accept itself. */
        bb_usleep(2000);

        bb_fi_fail_alloc_at(n);
        bb_async_connection_t *conn = bb_async_connection_accept(runtime, listener->connection->fd);
        bool injected = bb_fi_alloc_failures() > 0;
        bb_fi_reset();

        if (!injected)
        {
            fault_check(conn != NULL, "accept succeeds when nothing fails", n);
            bb_async_connection_destroy(conn);
            close(client);
            bb_async_connection_destroy(listener);
            bb_runtime_destroy(runtime);
            fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
            break;
        }

        failures_swept++;
        fault_check(conn == NULL, "a failed allocation yields NULL", n);

        /* Whatever happened to that client, accepting works afterwards. */
        int second = raw_client();
        bb_async_connection_t *ok = accept_one(runtime, listener);
        fault_check(ok != NULL, "accept works after the failure", n);
        bb_async_connection_destroy(ok);

        close(second);
        close(client);
        bb_async_connection_destroy(listener);
        bb_runtime_destroy(runtime);
        fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
    }

    BB_ASSERT(failures_swept >= 2);
    printf("\t  (%u allocation points failed in turn)\n", failures_swept);
}

static void test_connect_allocation_sweep(void)
{
    printf("\tTesting bb_async_connection_connect() under allocation failures...\n");

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        int fds_before = fault_count_open_fds();

        bb_runtime_t *runtime = bb_runtime_create();
        BB_ASSERT(runtime);
        bb_async_connection_t *listener = bb_async_connection_serve(runtime, TEST_PORT);
        BB_ASSERT(listener);

        bb_fi_fail_alloc_at(n);
        bb_async_connection_t *conn = bb_async_connection_connect(runtime, "127.0.0.1", TEST_PORT_STR);
        bool injected = bb_fi_alloc_failures() > 0;
        bb_fi_reset();

        if (!injected)
        {
            fault_check(conn != NULL, "connect succeeds when nothing fails", n);
            bb_async_connection_destroy(conn);
            bb_async_connection_destroy(listener);
            bb_runtime_destroy(runtime);
            fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
            break;
        }

        failures_swept++;
        fault_check(conn == NULL, "a failed allocation yields NULL", n);

        bb_async_connection_destroy(listener);
        bb_runtime_destroy(runtime);
        fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
    }

    BB_ASSERT(failures_swept >= 2);
    printf("\t  (%u allocation points failed in turn)\n", failures_swept);
}

/* ============================================================
 * Read/write task scheduling: registration failures
 * ============================================================ */

/* A connected pair (client side wrapped in an async connection). */
typedef struct
{
    bb_runtime_t *runtime;
    bb_async_connection_t *listener;
    bb_async_connection_t *conn;
    int peer;
} pair_t;

static void pair_init(pair_t *p)
{
    p->runtime = bb_runtime_create();
    BB_ASSERT(p->runtime);
    p->listener = bb_async_connection_serve(p->runtime, TEST_PORT);
    BB_ASSERT(p->listener);
    p->peer = raw_client();
    p->conn = accept_one(p->runtime, p->listener);
    BB_ASSERT(p->conn);
}

static void pair_destroy(pair_t *p)
{
    bb_async_connection_destroy(p->conn);
    close(p->peer);
    bb_async_connection_destroy(p->listener);
    bb_runtime_destroy(p->runtime);
}

static void test_read_task_registration_failure(void)
{
    printf("\tTesting bb_async_connection_create_read_task() under registration failure...\n");

    int fds_before = fault_count_open_fds();

    pair_t p;
    pair_init(&p);

    bb_fi_fail_watch_at(1);
    bb_error_t err = bb_async_connection_create_read_task(p.conn, dummy_read_step, dummy_read_error, NULL);
    BB_ASSERT(bb_fi_watch_failures() == 1);
    bb_fi_reset();

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(p.conn->read_task == NULL);

    /* Unchanged, so the same call can be retried and then succeeds. */
    err = bb_async_connection_create_read_task(p.conn, dummy_read_step, dummy_read_error, NULL);
    BB_ASSERT(!BB_FAILED(err));
    BB_ASSERT(p.conn->read_task != NULL);

    pair_destroy(&p);
    BB_ASSERT(fault_count_open_fds() == fds_before);
}

static void test_write_task_registration_failure(void)
{
    printf("\tTesting bb_async_connection_create_write_task() under registration failure...\n");

    int fds_before = fault_count_open_fds();

    pair_t p;
    pair_init(&p);

    bb_fi_fail_watch_at(1);
    bb_error_t err = bb_async_connection_create_write_task(p.conn, dummy_write_done, dummy_write_done, NULL);
    BB_ASSERT(bb_fi_watch_failures() == 1);
    bb_fi_reset();

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(p.conn->write_task == NULL);

    /* A failed schedule must not leave a write marked as in flight, or the
     * retry below would be silently swallowed as "already writing". */
    BB_ASSERT(!p.conn->connection->write_pending);

    err = bb_async_connection_create_write_task(p.conn, dummy_write_done, dummy_write_done, NULL);
    BB_ASSERT(!BB_FAILED(err));
    BB_ASSERT(p.conn->write_task != NULL);
    BB_ASSERT(p.conn->connection->write_pending);

    pair_destroy(&p);
    BB_ASSERT(fault_count_open_fds() == fds_before);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    BB_ASSERT(bb_platform_net_init() == 0);

    printf("Running async connection failure-injection tests...\n");

    warm_up();

    test_create_allocation_sweep();
    test_serve_allocation_sweep();
    test_accept_allocation_sweep();
    test_connect_allocation_sweep();
    test_read_task_registration_failure();
    test_write_task_registration_failure();

    printf("All async connection failure-injection tests passed.\n");
    return 0;
}
