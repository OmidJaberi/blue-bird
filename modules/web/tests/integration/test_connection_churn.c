#include "connection/connection.h"

#include <blue-bird/utils/platform.h>
#include <blue-bird/utils/time.h>
#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if !defined(_WIN32)
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#endif

/*
 * Connection-layer stress tests: high concurrent connection churn
 * ==============================================================
 *
 * test_connection_integration.c establishes a single connection and
 * tears it down. That never exercises what a loaded server actually
 * does -- accept hundreds of connections, hold them concurrently, and
 * destroy them continuously -- and it never exercises the state that
 * bb_connection_accept() keeps *between* calls.
 *
 * Two things here are process-global and therefore only observable
 * under sustained churn:
 *
 *   - The reserved "spare" descriptor (_bb_connection_spare_fd). It is
 *     created lazily on the first accept and re-armed after each
 *     rejection. A bug in that dance leaks one descriptor per
 *     exhaustion event, which a single-connection test can never see
 *     but which kills a long-running server.
 *
 *   - Descriptor accounting in general. Every bb_connection_create()
 *     that fails partway, and every bb_connection_destroy(), has to
 *     leave the descriptor table exactly as it found it. One leaked fd
 *     per connection is invisible at N=1 and fatal at N=100k.
 *
 * So the assertions below are mostly about descriptor counts before and
 * after thousands of accept/destroy cycles, not about any single
 * connection's behaviour.
 */

#if !defined(_WIN32)

/* Concurrent connections held open at once. Two descriptors each
 * (client + accepted server side), plus the listener. */
#define CHURN_CONCURRENT 100

/* Sequential accept/destroy cycles in the fd-leak test. Large enough
 * that a one-descriptor-per-cycle leak would blow well past any
 * plausible RLIMIT_NOFILE. */
#define CHURN_CYCLES 500

/* ======================================================================= */
/* Descriptor accounting                                                   */
/* ======================================================================= */

/*
 * Counts descriptors actually open in this process, by walking
 * /proc/self/fd rather than probing numbers with fcntl(). The
 * descriptor table has gaps under churn -- closed connections leave
 * holes that later ones refill -- so "highest fd number" is not a
 * usable proxy for "how many are open".
 */
static int _open_fd_count(void)
{
    DIR *dir = opendir("/proc/self/fd");

    if (!dir)
    {
        /* macOS and the BSDs have no /proc, but /dev/fd (fdescfs) lists
         * the calling process's open descriptors the same way. */
        dir = opendir("/dev/fd");
    }

    if (!dir)
    {
        return -1; // neither is available
    }

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
        {
            continue; // "." and ".."
        }
        count++;
    }

    closedir(dir);

    /* opendir() itself holds one descriptor for the duration of the
     * walk, so it counted itself. */
    return count - 1;
}

static bb_socket_t _create_listener(int *out_port)
{
    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);

    if (bb_socket_is_invalid(fd))
    {
        return BB_INVALID_SOCKET;
    }

    bb_socket_set_nonblocking(fd);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        bb_socket_close(fd);
        return BB_INVALID_SOCKET;
    }

    /* A deep backlog is the point: churn tests want many connections
     * genuinely queued at once, not serialised by a backlog of 1. */
    if (listen(fd, 128) != 0)
    {
        bb_socket_close(fd);
        return BB_INVALID_SOCKET;
    }

    socklen_t len = sizeof(addr);

    if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0)
    {
        bb_socket_close(fd);
        return BB_INVALID_SOCKET;
    }

    *out_port = ntohs(addr.sin_port);

    return fd;
}

static bb_socket_t _connect_to(int port)
{
    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);

    if (bb_socket_is_invalid(fd))
    {
        return BB_INVALID_SOCKET;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        bb_socket_close(fd);
        return BB_INVALID_SOCKET;
    }

    return fd;
}

/*
 * The listener is non-blocking, so a connection that has been connect()ed
 * may not have completed in the kernel by the time we call accept().
 * Retries rather than treating the first EAGAIN as failure.
 *
 * Bounded by wall-clock time, not a fixed attempt count: a fixed count
 * at a fixed sleep (the previous version used 200 attempts * 1ms = a
 * hard 200ms) is exactly the kind of budget that is comfortable on an
 * idle dev machine and occasionally too tight on a loaded CI runner --
 * that was the source of the intermittent failures. Backing off from 1ms
 * towards 20ms trades a little latency in the rare slow case for not
 * spinning at 1ms resolution for the whole budget in the common one.
 */
static bb_connection_t *_accept_retrying(bb_socket_t listener, int budget_ms)
{
    int64_t deadline = bb_time_monotonic_ms() + budget_ms;
    unsigned int sleep_us = 1000;

    for (;;)
    {
        bb_connection_t *conn = bb_connection_accept(listener);

        if (conn)
        {
            return conn;
        }

        if (bb_time_monotonic_ms() >= deadline)
        {
            return NULL;
        }

        bb_usleep(sleep_us);

        if (sleep_us < 20000)
        {
            sleep_us *= 2;
        }
    }
}

/*
 * Reads until the connection reports the peer's close, or the budget runs
 * out. A single bb_connection_read() is not enough to observe an EOF that
 * was sent *before* accept(): on Linux the FIN is processed synchronously
 * inside the peer's close(), so it is already queued by the time accept()
 * returns. macOS delivers loopback packets asynchronously, so accept() can
 * succeed off the handshake ACK while the FIN behind it is still in flight,
 * and the first read legitimately sees "no data yet" (state stays READING).
 */
static bb_error_t _read_until_closed(bb_connection_t *conn, int budget_ms)
{
    int64_t deadline = bb_time_monotonic_ms() + budget_ms;
    bb_error_t err = bb_connection_read(conn);

    while (err.code == BB_OK &&
           conn->state != BB_CONNECTION_CLOSED &&
           bb_time_monotonic_ms() < deadline)
    {
        bb_usleep(1000);
        err = bb_connection_read(conn);
    }

    return err;
}

/* ======================================================================= */
/* Tests                                                                   */
/* ======================================================================= */

/*
 * Hold many accepted connections open simultaneously, then tear them
 * all down.
 *
 * Each bb_connection_t owns a descriptor, a read buffer, and a
 * transport, all allocated at accept time. Holding CHURN_CONCURRENT of
 * them at once means every one of those allocations is live
 * simultaneously -- so a connection that shares state it shouldn't
 * (a static buffer, a reused transport) shows up as cross-talk here,
 * and the descriptor count confirms the whole batch is released.
 */
static void test_churn_many_concurrent_accepts(void)
{
    printf("\tRunning test_churn_many_concurrent_accepts...\n");

    int port = 0;
    bb_socket_t listener = _create_listener(&port);
    BB_ASSERT(!bb_socket_is_invalid(listener));

    /* Baseline taken after the listener exists, so it isn't counted as
     * a leak. The first accept lazily opens the spare descriptor, so
     * prime that before measuring too -- it is a permanent reserve, not
     * a per-connection cost. */
    bb_socket_t primer_client = _connect_to(port);
    BB_ASSERT(!bb_socket_is_invalid(primer_client));

    bb_connection_t *primer = _accept_retrying(listener, 2000);
    BB_ASSERT(primer != NULL);
    bb_connection_destroy(primer);
    bb_socket_close(primer_client);

    int baseline_fds = _open_fd_count();

    bb_socket_t *clients = calloc(CHURN_CONCURRENT, sizeof(*clients));
    bb_connection_t **servers = calloc(CHURN_CONCURRENT, sizeof(*servers));
    BB_ASSERT(clients != NULL && servers != NULL);

    int established = 0;

    for (int i = 0; i < CHURN_CONCURRENT; i++)
    {
        clients[i] = _connect_to(port);

        if (bb_socket_is_invalid(clients[i]))
        {
            break; // ran out of descriptors; test what we got
        }

        servers[i] = _accept_retrying(listener, 2000);

        if (!servers[i])
        {
            bb_socket_close(clients[i]);
            clients[i] = BB_INVALID_SOCKET;
            break;
        }

        established++;
    }

    printf("\t\theld %d concurrent connections\n", established);

    BB_ASSERT(established > 0);

    /* Every accepted connection must be a distinct object on a distinct
     * descriptor. A duplicate fd here would mean accept() handed the
     * same descriptor out twice -- i.e. something double-closed. */
    for (int i = 0; i < established; i++)
    {
        BB_ASSERT(servers[i] != NULL);
        BB_ASSERT(!bb_socket_is_invalid(servers[i]->fd));

        for (int j = i + 1; j < established; j++)
        {
            BB_ASSERT(servers[i] != servers[j]);
            BB_ASSERT(servers[i]->fd != servers[j]->fd);
        }
    }

    for (int i = 0; i < established; i++)
    {
        bb_connection_destroy(servers[i]);
        bb_socket_close(clients[i]);
    }

    free(servers);
    free(clients);

    /* Back to baseline: destroying a connection must release its
     * descriptor, and closing the clients must release theirs. */
    if (baseline_fds >= 0)
    {
        int after_fds = _open_fd_count();
        BB_ASSERT(after_fds == baseline_fds);
    }

    bb_socket_close(listener);
}

/*
 * Sustained accept -> destroy cycling on a single listener.
 *
 * This is the churn pattern that actually breaks servers: not many
 * connections at once, but an endless stream of short-lived ones. Any
 * per-connection descriptor leak, and any failure to re-arm the spare
 * descriptor, accumulates linearly here. Comparing the descriptor count
 * against the baseline after CHURN_CYCLES full cycles catches a leak of
 * even one descriptor per cycle.
 */
static void test_churn_sequential_accept_destroy_leaks_nothing(void)
{
    printf("\tRunning test_churn_sequential_accept_destroy_leaks_nothing...\n");

    int port = 0;
    bb_socket_t listener = _create_listener(&port);
    BB_ASSERT(!bb_socket_is_invalid(listener));

    /* Prime the lazily-created spare descriptor before the baseline. */
    bb_socket_t primer_client = _connect_to(port);
    BB_ASSERT(!bb_socket_is_invalid(primer_client));

    bb_connection_t *primer = _accept_retrying(listener, 2000);
    BB_ASSERT(primer != NULL);
    bb_connection_destroy(primer);
    bb_socket_close(primer_client);

    int baseline_fds = _open_fd_count();

    int completed = 0;

    for (int i = 0; i < CHURN_CYCLES; i++)
    {
        bb_socket_t client = _connect_to(port);

        if (bb_socket_is_invalid(client))
        {
            break;
        }

        bb_connection_t *server = _accept_retrying(listener, 2000);

        if (!server)
        {
            bb_socket_close(client);
            break;
        }

        bb_connection_destroy(server);
        bb_socket_close(client);

        completed++;

        /*
         * Check partway through as well as at the end. A leak that only
         * starts after the descriptor table gets fragmented would still
         * be caught, and the failure points at roughly where it began.
         */
        if (baseline_fds >= 0 && (i % 100) == 99)
        {
            BB_ASSERT(_open_fd_count() == baseline_fds);
        }
    }

    printf("\t\tcompleted %d accept/destroy cycles\n", completed);

    BB_ASSERT(completed == CHURN_CYCLES);

    if (baseline_fds >= 0)
    {
        BB_ASSERT(_open_fd_count() == baseline_fds);
    }

    bb_socket_close(listener);
}

/*
 * Accepting from a listener whose backlog is deeply queued.
 *
 * Connections are opened first and accepted only afterwards, so the
 * kernel holds a real backlog rather than handing them over one at a
 * time. bb_connection_accept() must drain the whole queue across
 * successive calls and then report empty cleanly (NULL, not an error
 * spin) -- the same contract the event loop's accept task relies on to
 * know when to stop.
 */
static void test_churn_drains_deep_backlog(void)
{
    printf("\tRunning test_churn_drains_deep_backlog...\n");

    int port = 0;
    bb_socket_t listener = _create_listener(&port);
    BB_ASSERT(!bb_socket_is_invalid(listener));

    const int queued = 64;

    bb_socket_t *clients = calloc(queued, sizeof(*clients));
    BB_ASSERT(clients != NULL);

    int opened = 0;

    for (int i = 0; i < queued; i++)
    {
        clients[i] = _connect_to(port);

        if (bb_socket_is_invalid(clients[i]))
        {
            break;
        }
        opened++;
    }

    BB_ASSERT(opened > 0);

    /* Give the kernel a moment to finish the handshakes now sitting in
     * the backlog, so the drain below isn't racing them. */
    bb_usleep(50000);

    bb_connection_t **accepted = calloc(opened, sizeof(*accepted));
    BB_ASSERT(accepted != NULL);

    int drained = 0;

    /* Wall-clock bounded rather than attempt-count bounded, for the
     * same reason as _accept_retrying(): a fixed attempt count at a
     * fixed sleep is tight on a loaded CI runner even though it is
     * comfortable locally. Still bounded -- if accept() ever started
     * returning connections that weren't queued, this stops rather
     * than looping forever. */
    int64_t deadline = bb_time_monotonic_ms() + 2000;

    while (drained < opened && bb_time_monotonic_ms() < deadline)
    {
        bb_connection_t *conn = bb_connection_accept(listener);

        if (conn)
        {
            accepted[drained++] = conn;
        }
        else
        {
            bb_usleep(1000);
        }
    }

    printf("\t\tdrained %d of %d queued connections\n", drained, opened);

    BB_ASSERT(drained == opened);

    /* Backlog is empty now: accept() must report that plainly rather
     * than producing a phantom connection. */
    BB_ASSERT(bb_connection_accept(listener) == NULL);
    BB_ASSERT(bb_connection_accept(listener) == NULL);

    for (int i = 0; i < drained; i++)
    {
        bb_connection_destroy(accepted[i]);
    }

    for (int i = 0; i < opened; i++)
    {
        bb_socket_close(clients[i]);
    }

    free(accepted);
    free(clients);

    bb_socket_close(listener);
}

/*
 * Churn where the peer disconnects immediately after connecting.
 *
 * Load balancers, health checks, and port scanners all produce this:
 * the client is gone before the server ever reads. The accepted
 * connection is therefore born already at EOF. bb_connection_read()
 * must report that as an orderly close (BB_CONNECTION_CLOSED, success
 * -- not an I/O error), and destroying it must still release the
 * descriptor cleanly, thousands of times over.
 */
static void test_churn_immediate_peer_disconnect(void)
{
    printf("\tRunning test_churn_immediate_peer_disconnect...\n");

    int port = 0;
    bb_socket_t listener = _create_listener(&port);
    BB_ASSERT(!bb_socket_is_invalid(listener));

    bb_socket_t primer_client = _connect_to(port);
    BB_ASSERT(!bb_socket_is_invalid(primer_client));

    bb_connection_t *primer = _accept_retrying(listener, 2000);
    BB_ASSERT(primer != NULL);
    bb_connection_destroy(primer);
    bb_socket_close(primer_client);

    int baseline_fds = _open_fd_count();

    const int cycles = 200;
    int closed_observed = 0;

    for (int i = 0; i < cycles; i++)
    {
        bb_socket_t client = _connect_to(port);
        BB_ASSERT(!bb_socket_is_invalid(client));

        /* Hang up before the server has accepted. */
        bb_socket_close(client);

        bb_connection_t *server = _accept_retrying(listener, 2000);
        BB_ASSERT(server != NULL);

        bb_error_t err = _read_until_closed(server, 2000);

        /* EOF is not a failure. The read either drains what the peer
         * managed to send and reports success, or observes the close --
         * either way the connection must end up CLOSED, never wedged in
         * READING with an I/O error. */
        BB_ASSERT(err.code == BB_OK);

        if (server->state == BB_CONNECTION_CLOSED)
        {
            closed_observed++;
        }

        bb_connection_destroy(server);
    }

    printf("\t\tobserved orderly close on %d of %d connections\n", closed_observed, cycles);

    /* The peer closed every time, so every connection should have seen
     * it. Allowing a small shortfall would only mask a real regression. */
    BB_ASSERT(closed_observed == cycles);

    if (baseline_fds >= 0)
    {
        BB_ASSERT(_open_fd_count() == baseline_fds);
    }

    bb_socket_close(listener);
}

/*
 * Interleaved churn: new connections accepted while older ones are
 * still being destroyed, with the live set staying roughly constant.
 *
 * Steady-state load, rather than the batch-up/batch-down shapes above.
 * Descriptor numbers get recycled continuously here -- a connection
 * destroyed this iteration hands its number to the one accepted next --
 * so anything that caches or keys on a raw fd across connection
 * lifetimes surfaces as cross-talk between two unrelated connections.
 */
static void test_churn_interleaved_accept_and_destroy(void)
{
    printf("\tRunning test_churn_interleaved_accept_and_destroy...\n");

    int port = 0;
    bb_socket_t listener = _create_listener(&port);
    BB_ASSERT(!bb_socket_is_invalid(listener));

    const int live_target = 32;
    const int iterations = 300;

    bb_socket_t *clients = calloc(live_target, sizeof(*clients));
    bb_connection_t **servers = calloc(live_target, sizeof(*servers));
    BB_ASSERT(clients != NULL && servers != NULL);

    for (int i = 0; i < live_target; i++)
    {
        clients[i] = BB_INVALID_SOCKET;
        servers[i] = NULL;
    }

    int accepted_total = 0;

    for (int i = 0; i < iterations; i++)
    {
        int slot = i % live_target;

        /* Retire whatever is occupying this slot, then immediately
         * refill it -- so a destroy and an accept are always adjacent. */
        if (servers[slot])
        {
            bb_connection_destroy(servers[slot]);
            bb_socket_close(clients[slot]);
            servers[slot] = NULL;
            clients[slot] = BB_INVALID_SOCKET;
        }

        clients[slot] = _connect_to(port);
        BB_ASSERT(!bb_socket_is_invalid(clients[slot]));

        servers[slot] = _accept_retrying(listener, 2000);
        BB_ASSERT(servers[slot] != NULL);

        accepted_total++;

        /* The connection just accepted must not collide with any other
         * connection still live in the set -- the fd it was handed was
         * very likely recycled from one destroyed moments ago. */
        for (int j = 0; j < live_target; j++)
        {
            if (j == slot || !servers[j])
            {
                continue;
            }
            BB_ASSERT(servers[j]->fd != servers[slot]->fd);
            BB_ASSERT(servers[j] != servers[slot]);
        }
    }

    printf("\t\taccepted %d connections through a %d-connection live set\n",
           accepted_total, live_target);

    BB_ASSERT(accepted_total == iterations);

    for (int i = 0; i < live_target; i++)
    {
        if (servers[i])
        {
            bb_connection_destroy(servers[i]);
        }
        if (!bb_socket_is_invalid(clients[i]))
        {
            bb_socket_close(clients[i]);
        }
    }

    free(servers);
    free(clients);

    bb_socket_close(listener);
}

#endif /* !_WIN32 */

int main(void)
{
    printf("Starting connection churn stress test...\n");

#if !defined(_WIN32)
    BB_ASSERT(bb_platform_net_init() == 0);

    test_churn_many_concurrent_accepts();
    test_churn_sequential_accept_destroy_leaks_nothing();
    test_churn_drains_deep_backlog();
    test_churn_immediate_peer_disconnect();
    test_churn_interleaved_accept_and_destroy();

    bb_platform_net_cleanup();
#endif

    printf("Connection churn stress test passed.\n");
    return 0;
}
