/*
 * Failure injection for the server: bb_server_start() and the accept ->
 * read-task -> respond path.
 *
 * Each sweep fails the 1st, 2nd, 3rd ... allocation (or runtime registration)
 * on the path in turn, until a run completes without the failure ever firing.
 * Every failed run must leave the server healthy, release everything it
 * acquired (LeakSanitizer / AddressSanitizer catch leaks and use-after-free at
 * the point of the bad error path) and leave no descriptor behind.
 *
 * See fault/fault_inject.h for how the failures are injected.
 */
#include "fault_inject.h"
#include "fault_util.h"

#include "server_internal.h"

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>
#include <blue-bird/utils/time.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TEST_PORT 19473

/* A sweep stops by itself once an injection no longer fires; this only bounds
 * a runaway. */
#define MAX_SWEEP 1000

/* ============================================================
 * Helpers
 * ============================================================ */

static bb_error_t ping_handler(bb_request_t *req, bb_response_t *res)
{
    (void)req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "pong");
    return BB_SUCCESS();
}

static void noop_timer(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

static int client_open(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(fd >= 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    BB_ASSERT(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);

    /* Completes against the listen backlog; the server need not have accepted. */
    BB_ASSERT(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    BB_ASSERT(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);

    return fd;
}

static void client_send_ping(int fd, bool close_after)
{
    static const char persistent[] = "GET /ping HTTP/1.1\r\nHost: localhost\r\n\r\n";
    static const char closing[] = "GET /ping HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";

    const char *request = close_after ? closing : persistent;
    size_t len = close_after ? sizeof(closing) - 1 : sizeof(persistent) - 1;
    BB_ASSERT(send(fd, request, len, 0) == (ssize_t)len);
}

/* A connection that is read through to its end: one request, "Connection: close". */
static int client_connect(void)
{
    int fd = client_open();
    client_send_ping(fd, true);
    return fd;
}

typedef enum
{
    OUTCOME_RESPONSE, /* a full 200 "pong" answer */
    OUTCOME_DROPPED,  /* the server closed the connection without a good answer */
    OUTCOME_STALLED   /* neither answered nor closed within the deadline */
} outcome_t;

/* Drives the server's runtime until the client sees the connection end. */
static outcome_t client_await(bb_runtime_t *runtime, int fd, int timeout_ms)
{
    char buf[1024];
    size_t len = 0;
    int64_t deadline = bb_time_monotonic_ms() + timeout_ms;

    for (;;)
    {
        bb_runtime_tick(runtime);

        ssize_t n = recv(fd, buf + len, sizeof(buf) - 1 - len, 0);
        if (n > 0)
        {
            len += (size_t)n;
        }
        else if (n == 0)
        {
            buf[len] = '\0';
            return (strstr(buf, " 200") && strstr(buf, "pong")) ? OUTCOME_RESPONSE : OUTCOME_DROPPED;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            return OUTCOME_DROPPED; /* reset */
        }

        if (bb_time_monotonic_ms() > deadline)
        {
            return OUTCOME_STALLED;
        }
    }
}

typedef struct
{
    bb_runtime_t *runtime;
    bb_server_t *server;
} fixture_t;

static void fixture_init(fixture_t *f)
{
    f->runtime = bb_runtime_create();
    BB_ASSERT(f->runtime);
    bb_runtime_set_running(f->runtime);

    /* Keeps bb_runtime_tick() from sleeping out the idle timeout. */
    BB_ASSERT(bb_runtime_set_interval(f->runtime, 1, noop_timer, NULL) != NULL);

    f->server = bb_server_create_on_runtime(f->runtime, TEST_PORT);
    BB_ASSERT(f->server);
    bb_server_add_route(f->server, "GET", "/ping", ping_handler);
}

static void fixture_destroy(fixture_t *f)
{
    bb_server_destroy(f->server);
    bb_runtime_destroy(f->runtime);
}

/* The server answers a fresh request correctly. */
static bool server_is_healthy(fixture_t *f)
{
    int fd = client_connect();
    outcome_t outcome = client_await(f->runtime, fd, 2000);
    close(fd);
    return outcome == OUTCOME_RESPONSE;
}

/* The first accept opens the process-lifetime spare descriptor; take that
 * out of the descriptor accounting. */
static void warm_up(void)
{
    bb_fi_reset();

    fixture_t f;
    fixture_init(&f);
    BB_ASSERT(!BB_FAILED(bb_server_start(f.server)));
    BB_ASSERT(server_is_healthy(&f));
    fixture_destroy(&f);
}

/* ============================================================
 * bb_server_start(): argument and state errors
 * ============================================================ */

static void test_start_rejects_bad_state(void)
{
    printf("\tTesting bb_server_start() reports argument and state errors...\n");

    bb_fi_reset();

    BB_ASSERT(bb_server_start(NULL).code == BB_ERR_NULL);

    fixture_t f;
    fixture_init(&f);

    /* No listening socket. */
    bb_async_connection_t *listener = f.server->async_conn;
    f.server->async_conn = NULL;
    BB_ASSERT(bb_server_start(f.server).code == BB_ERR_INTERNAL);
    BB_ASSERT(f.server->accept_task == NULL);
    f.server->async_conn = listener;

    /* Started once: starting again is an error and must not disturb the
     * running accept task (it used to overwrite it, leaking the old data). */
    BB_ASSERT(!BB_FAILED(bb_server_start(f.server)));
    bb_task_t *task = f.server->accept_task;
    bb_server_task_data_t *data = f.server->accept_task_data;
    BB_ASSERT(task && data);

    bb_error_t again = bb_server_start(f.server);
    BB_ASSERT(again.code == BB_ERR_INTERNAL);
    BB_ASSERT(f.server->accept_task == task);
    BB_ASSERT(f.server->accept_task_data == data);
    BB_ASSERT(server_is_healthy(&f));

    fixture_destroy(&f);
}

/* ============================================================
 * bb_server_start(): allocation failures
 * ============================================================ */

static void test_start_allocation_sweep(void)
{
    printf("\tTesting bb_server_start() under allocation failures...\n");

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        int fds_before = fault_count_open_fds();

        fixture_t f;
        fixture_init(&f);

        bb_fi_fail_alloc_at(n);
        bb_error_t err = bb_server_start(f.server);
        bool injected = bb_fi_alloc_failures() > 0;
        bb_fi_reset();

        if (!injected)
        {
            /* Past the last allocation on the path: a plain success. */
            fault_check(!BB_FAILED(err), "start succeeds when nothing fails", n);
            fixture_destroy(&f);
            fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
            break;
        }

        failures_swept++;

        fault_check(BB_FAILED(err), "a failed allocation is reported", n);
        fault_check(err.code == BB_ERR_ALLOC, "reported as BB_ERR_ALLOC", n);
        fault_check(f.server->accept_task == NULL, "no accept task left behind", n);
        fault_check(f.server->accept_task_data == NULL, "no task data left behind", n);

        /* Not started, so retrying works and the server then serves. */
        fault_check(!BB_FAILED(bb_server_start(f.server)), "start can be retried", n);
        fault_check(server_is_healthy(&f), "server serves after a retried start", n);

        fixture_destroy(&f);
        fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);
    }

    /* If the interposition silently stopped working this sweep would pass
     * vacuously; the path has at least the task-data allocation. */
    BB_ASSERT(failures_swept >= 1);
    printf("\t  (%u allocation points failed in turn)\n", failures_swept);
}

/* ============================================================
 * bb_server_start(): registration failures
 * ============================================================ */

static void test_start_registration_failure(void)
{
    printf("\tTesting bb_server_start() under runtime registration failure...\n");

    int fds_before = fault_count_open_fds();

    fixture_t f;
    fixture_init(&f);

    bb_fi_fail_watch_at(1);
    bb_error_t err = bb_server_start(f.server);
    BB_ASSERT(bb_fi_watch_failures() == 1);
    bb_fi_reset();

    /* The failure used to be logged and swallowed: callers believed the
     * server was running while nothing watched the listening socket. */
    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_INTERNAL);
    BB_ASSERT(f.server->accept_task == NULL);
    BB_ASSERT(f.server->accept_task_data == NULL);

    /* Nothing is accepting: a client connects (the kernel backlog) but is
     * never answered. */
    int fd = client_connect();
    BB_ASSERT(client_await(f.runtime, fd, 100) == OUTCOME_STALLED);
    close(fd);

    /* The server is unharmed: starting again works and it serves. */
    BB_ASSERT(!BB_FAILED(bb_server_start(f.server)));
    BB_ASSERT(server_is_healthy(&f));

    fixture_destroy(&f);
    BB_ASSERT(fault_count_open_fds() == fds_before);
}

/* ============================================================
 * Accept path: allocation and registration sweeps
 * ============================================================ */

typedef struct
{
    bool injected;
    outcome_t first;
} accept_run_t;

/* One server, one request with a failure armed, then a request with nothing
 * armed to prove the server survived. */
static accept_run_t run_accept_iteration(unsigned fail_alloc_at, unsigned fail_watch_at, unsigned n)
{
    accept_run_t run = { false, OUTCOME_STALLED };
    int fds_before = fault_count_open_fds();

    fixture_t f;
    fixture_init(&f);
    fault_check(!BB_FAILED(bb_server_start(f.server)), "start succeeds", n);

    if (fail_alloc_at)
    {
        bb_fi_fail_alloc_at(fail_alloc_at);
    }
    if (fail_watch_at)
    {
        bb_fi_fail_watch_at(fail_watch_at);
    }

    int fd = client_connect();
    run.first = client_await(f.runtime, fd, 500);
    run.injected = bb_fi_alloc_failures() > 0 || bb_fi_watch_failures() > 0;
    bb_fi_reset();

    fault_check(run.first != OUTCOME_STALLED || !run.injected,
          "a connection hit by a failure is dropped or answered, never left hanging", n);
    close(fd);

    fault_check(server_is_healthy(&f), "server still serves after the failure", n);

    fixture_destroy(&f);
    fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);

    return run;
}

static void test_accept_allocation_sweep(void)
{
    printf("\tTesting accept/read/respond under allocation failures...\n");

    unsigned failures_swept = 0;
    unsigned dropped = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        accept_run_t run = run_accept_iteration(n, 0, n);
        if (!run.injected)
        {
            fault_check(run.first == OUTCOME_RESPONSE, "an uninjected run is answered", n);
            break;
        }
        failures_swept++;
        dropped += (run.first == OUTCOME_DROPPED);
    }

    /* The accept path allocates in many places; a handful of failure points
     * means the interposition is not reaching the library. */
    BB_ASSERT(failures_swept >= 8);
    printf("\t  (%u allocation points failed in turn; %u dropped the connection)\n", failures_swept, dropped);
}

static void test_accept_registration_sweep(void)
{
    printf("\tTesting accept/read/respond under runtime registration failures...\n");

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        accept_run_t run = run_accept_iteration(0, n, n);
        if (!run.injected)
        {
            fault_check(run.first == OUTCOME_RESPONSE, "an uninjected run is answered", n);
            break;
        }
        failures_swept++;
        fault_check(run.first == OUTCOME_DROPPED, "a connection that could not be registered is closed", n);
    }

    /* Per connection: the read watch, then the write watch. */
    BB_ASSERT(failures_swept >= 2);
    printf("\t  (%u registration points failed in turn)\n", failures_swept);
}

/* ============================================================
 * Persistent connections: allocation, registration and timer failures
 * ============================================================ */

/* How many complete "pong" responses the client has seen, and whether the
 * server has closed the connection. */
typedef struct
{
    char data[2048];
    size_t len;
    unsigned responses;
    bool eof;
} reader_t;

static void reader_poll(reader_t *r, int fd)
{
    for (;;)
    {
        ssize_t n = recv(fd, r->data + r->len, sizeof(r->data) - 1 - r->len, 0);
        if (n > 0)
        {
            r->len += (size_t)n;
            r->data[r->len] = '\0';
            continue;
        }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
        {
            r->eof = true;
        }
        break;
    }

    r->responses = 0;
    for (const char *p = strstr(r->data, "pong"); p; p = strstr(p + 4, "pong"))
    {
        r->responses++;
    }
}

/* Drives the runtime until `want` responses have arrived, the server closes,
 * or the deadline passes. */
static void reader_await(bb_runtime_t *runtime, int fd, reader_t *r, unsigned want, int timeout_ms)
{
    int64_t deadline = bb_time_monotonic_ms() + timeout_ms;

    for (;;)
    {
        bb_runtime_tick(runtime);
        reader_poll(r, fd);

        if (r->responses >= want || r->eof || bb_time_monotonic_ms() > deadline)
        {
            return;
        }
    }
}

typedef struct
{
    bool injected;
    unsigned answered; /* responses received before the connection ended */
    bool first_said_close;
} keepalive_run_t;

/* Two requests down one persistent connection, with a failure armed. */
static keepalive_run_t run_keepalive_iteration(unsigned fail_alloc_at, unsigned fail_watch_at, unsigned fail_timer_at, unsigned n)
{
    keepalive_run_t run = { false, 0, false };
    int fds_before = fault_count_open_fds();

    fixture_t f;
    fixture_init(&f);
    fault_check(!BB_FAILED(bb_server_start(f.server)), "start succeeds", n);

    if (fail_alloc_at) bb_fi_fail_alloc_at(fail_alloc_at);
    if (fail_watch_at) bb_fi_fail_watch_at(fail_watch_at);
    if (fail_timer_at) bb_fi_fail_timer_at(fail_timer_at);

    int fd = client_open();
    reader_t reader;
    memset(&reader, 0, sizeof(reader));

    client_send_ping(fd, false);
    reader_await(f.runtime, fd, &reader, 1, 500);
    run.first_said_close = strstr(reader.data, "Connection: close") != NULL;

    if (reader.responses >= 1 && !reader.eof)
    {
        client_send_ping(fd, false);
        reader_await(f.runtime, fd, &reader, 2, 500);
    }

    run.answered = reader.responses;
    run.injected = bb_fi_alloc_failures() + bb_fi_watch_failures() + bb_fi_timer_failures() > 0;
    bool stalled = !reader.eof && reader.responses < 2;
    bb_fi_reset();

    fault_check(!(run.injected && stalled), "a connection hit by a failure is closed, never left hanging half-served", n);

    close(fd);

    fault_check(server_is_healthy(&f), "server still serves after the failure", n);

    fixture_destroy(&f);
    fault_check(fault_count_open_fds() == fds_before, "no descriptor leaked", n);

    return run;
}

static void test_keepalive_allocation_sweep(void)
{
    printf("\tTesting persistent connections under allocation failures...\n");

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        keepalive_run_t run = run_keepalive_iteration(n, 0, 0, n);
        if (!run.injected)
        {
            fault_check(run.answered == 2, "an uninjected run answers both requests", n);
            break;
        }
        failures_swept++;
    }

    /* Two requests' worth of allocations, well beyond a single exchange. */
    BB_ASSERT(failures_swept >= 12);
    printf("\t  (%u allocation points failed in turn)\n", failures_swept);
}

static void test_keepalive_registration_sweep(void)
{
    printf("\tTesting persistent connections under runtime registration failures...\n");

    unsigned failures_swept = 0;

    for (unsigned n = 1; n <= MAX_SWEEP; n++)
    {
        keepalive_run_t run = run_keepalive_iteration(0, n, 0, n);
        if (!run.injected)
        {
            fault_check(run.answered == 2, "an uninjected run answers both requests", n);
            break;
        }
        failures_swept++;
    }

    /* Read and write watch for the first request, the read watch re-armed for
     * the next one, then the write watch for the second answer. */
    BB_ASSERT(failures_swept >= 4);
    printf("\t  (%u registration points failed in turn)\n", failures_swept);
}

/* No sweep timer means no way to reclaim an idle connection, so the server
 * must not keep one open: the first request is still answered, then it closes. */
static void test_keepalive_degrades_without_sweep_timer(void)
{
    printf("\tTesting a persistent connection degrades to close when the sweep timer cannot be created...\n");

    keepalive_run_t run = run_keepalive_iteration(0, 0, 1, 1);

    BB_ASSERT(run.injected);
    BB_ASSERT(run.answered == 1);
    BB_ASSERT(run.first_said_close);
}

/* ============================================================
 * Shutdown: scheduling failure
 * ============================================================ */

/* The shutdown is driven by a timer. If that cannot be scheduled, nothing may
 * have changed: the server keeps running and serving, and the call can simply
 * be repeated. */
static void test_shutdown_scheduling_failure(void)
{
    printf("\tTesting a shutdown whose timer cannot be scheduled leaves the server running...\n");

    int fds_before = fault_count_open_fds();

    fixture_t f;
    fixture_init(&f);
    BB_ASSERT(!BB_FAILED(bb_server_start(f.server)));

    bb_fi_fail_timer_at(1);
    bb_error_t err = bb_server_shutdown(f.server, 200, NULL, NULL);
    BB_ASSERT(bb_fi_timer_failures() == 1);
    bb_fi_reset();

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_INTERNAL);
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_RUNNING);
    BB_ASSERT(f.server->accept_task != NULL);
    BB_ASSERT(server_is_healthy(&f));

    /* Repeating it works, and runs to completion. */
    BB_ASSERT(!BB_FAILED(bb_server_shutdown(f.server, 200, NULL, NULL)));
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_DRAINING);

    int64_t deadline = bb_time_monotonic_ms() + 2000;
    while (bb_server_get_state(f.server) != BB_SERVER_STATE_STOPPED && bb_time_monotonic_ms() < deadline)
    {
        bb_runtime_tick(f.runtime);
    }
    BB_ASSERT(bb_server_get_state(f.server) == BB_SERVER_STATE_STOPPED);

    fixture_destroy(&f);
    BB_ASSERT(fault_count_open_fds() == fds_before);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    BB_ASSERT(bb_platform_net_init() == 0);

    printf("Running server failure-injection tests...\n");

    warm_up();

    test_start_rejects_bad_state();
    test_start_allocation_sweep();
    test_start_registration_failure();
    test_accept_allocation_sweep();
    test_accept_registration_sweep();
    test_keepalive_allocation_sweep();
    test_keepalive_registration_sweep();
    test_keepalive_degrades_without_sweep_timer();
    test_shutdown_scheduling_failure();

    printf("All server failure-injection tests passed.\n");
    return 0;
}
