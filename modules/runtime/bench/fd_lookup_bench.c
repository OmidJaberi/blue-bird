/*
 * Benchmark for the runtime's descriptor bookkeeping.
 *
 * Builds N connected socket pairs, watches one end of each, and times the four
 * operations a busy server leans on, at several values of N:
 *
 *   register   watch N descriptors
 *   dispatch   make all N readable and run the loop until every callback ran
 *   re-arm     cancel a watcher and watch the same descriptor again (what a
 *              keep-alive connection does after every response)
 *   cancel     cancel all N watchers
 *
 * If descriptor lookup is O(1) the per-operation cost stays flat as N grows;
 * if it is a scan, it grows in proportion to N. Run it from a Release build
 * (sanitizers distort the numbers):
 *
 *     cmake -B build-rel -DCMAKE_BUILD_TYPE=Release -DBB_BUILD_BENCHMARKS=ON
 *     cmake --build build-rel --target bb_runtime_fd_bench
 *     build-rel/bench/bb_runtime_fd_bench [N ...]
 *
 * POSIX only.
 */
#include "blue-bird/runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef struct
{
    int fd;
    int peer;
    bb_task_t *task;
} conn_t;

static size_t g_fired;

static void on_readable(bb_task_t *task, void *userdata)
{
    (void)task;
    conn_t *c = userdata;
    char byte;
    if (read(c->fd, &byte, 1) == 1)
    {
        g_fired++;
    }
}

/* Higher resolution than bb_time_monotonic_ms()'s whole milliseconds. */
static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void raise_fd_limit(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur < rl.rlim_max)
    {
        rl.rlim_cur = rl.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rl);
    }
}

static int run(size_t n)
{
    bb_runtime_t *rt = bb_runtime_create();
    if (!rt)
    {
        fprintf(stderr, "runtime create failed\n");
        return 1;
    }
    bb_runtime_set_running(rt);

    conn_t *conns = calloc(n, sizeof(*conns));
    if (!conns)
    {
        return 1;
    }

    size_t made = 0;
    for (; made < n; made++)
    {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
        {
            break;
        }
        conns[made].fd = sv[0];
        conns[made].peer = sv[1];
    }
    if (made < n)
    {
        fprintf(stderr, "only %zu of %zu socket pairs (raise ulimit -n)\n", made, n);
        n = made;
    }

    /* register */
    double t0 = now_us();
    for (size_t i = 0; i < n; i++)
    {
        conns[i].task = bb_runtime_watch_fd(rt, conns[i].fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, on_readable, &conns[i]);
        if (!conns[i].task)
        {
            fprintf(stderr, "watch failed at %zu\n", i);
            return 1;
        }
    }
    double register_us = now_us() - t0;

    /* dispatch: every descriptor becomes readable at once */
    for (size_t i = 0; i < n; i++)
    {
        if (write(conns[i].peer, "x", 1) != 1)
        {
            return 1;
        }
    }
    g_fired = 0;
    t0 = now_us();
    int ticks = 0;
    while (g_fired < n && ticks < 100000)
    {
        bb_runtime_tick(rt);
        ticks++;
    }
    double dispatch_us = now_us() - t0;
    if (g_fired != n)
    {
        fprintf(stderr, "dispatch: %zu of %zu fired\n", g_fired, n);
        return 1;
    }

    /* re-arm: cancel + watch the same descriptor again, spread over the table */
    size_t ops = 20000;
    t0 = now_us();
    for (size_t k = 0; k < ops; k++)
    {
        conn_t *c = &conns[(k * 2654435761u) % n];
        bb_runtime_cancel_task(rt, c->task);
        c->task = bb_runtime_watch_fd(rt, c->fd, BB_EVENT_READ, BB_WATCH_PERSISTENT, on_readable, c);
        if (!c->task)
        {
            return 1;
        }
        if ((k & 255) == 255)
        {
            bb_runtime_tick(rt); /* lets the cancelled tasks be reclaimed */
        }
    }
    double rearm_us = now_us() - t0;

    /* cancel everything */
    t0 = now_us();
    for (size_t i = 0; i < n; i++)
    {
        bb_runtime_cancel_task(rt, conns[i].task);
    }
    double cancel_us = now_us() - t0;

    printf("%7zu | register %9.1f us (%6.2f us/op) | dispatch %10.1f us (%6.2f us/ev, %d ticks) | re-arm %6.2f us/op | cancel %6.2f us/op\n",
           n, register_us, register_us / (double)n, dispatch_us, dispatch_us / (double)n, ticks,
           rearm_us / (double)ops, cancel_us / (double)n);

    bb_runtime_destroy(rt);
    for (size_t i = 0; i < n; i++)
    {
        close(conns[i].fd);
        close(conns[i].peer);
    }
    free(conns);
    return 0;
}

int main(int argc, char **argv)
{
    raise_fd_limit();

    size_t default_sizes[] = { 500, 1000, 2000, 4000, 8000 };
    printf("runtime descriptor bookkeeping (lower is better; flat across rows = O(1))\n");

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            if (run((size_t)strtoul(argv[i], NULL, 10)) != 0)
            {
                return 1;
            }
        }
        return 0;
    }

    for (size_t i = 0; i < sizeof(default_sizes) / sizeof(default_sizes[0]); i++)
    {
        if (run(default_sizes[i]) != 0)
        {
            return 1;
        }
    }
    return 0;
}
