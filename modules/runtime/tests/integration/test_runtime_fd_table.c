#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/utils/platform.h"
#include "runtime_internal.h"

/* struct bb_poller is opaque in poller.h; these tests read the poller's own fd
 * bookkeeping directly to check it stays in step with the watcher table. */
#include "poller_backend.h"

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

/*
 * The runtime keeps its fd watchers in a hash table keyed by fd, and every task
 * holds a direct pointer to its own watcher. These tests target what can go
 * wrong with that: a rehash that mangles chains, several watchers sharing one
 * descriptor, replacing a watcher, a one-shot watcher unlinking itself while
 * the dispatch loop is walking its chain, removing one fd's watchers without
 * touching a neighbour that hashes next to it, and cleanup callbacks that call
 * back into the runtime while it is mid-removal.
 *
 * Descriptors come from socketpair() through the platform shim, as in the churn
 * tests: cheap, no listener, portable.
 */

#define MANY_PAIRS 1500
#define MEDIUM_PAIRS 300

typedef struct
{
    bb_socket_t client; /* watched */
    bb_socket_t server; /* written to, to make client readable */
} pair_t;

typedef struct
{
    bb_socket_t fd;
    int fired;
} probe_t;

static void noop_timer(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

/* Counts, and leaves the descriptor readable. */
static void probe_fire(bb_task_t *task, void *userdata)
{
    (void)task;
    ((probe_t *)userdata)->fired++;
}

/* Counts and consumes the byte, so a persistent watcher does not fire forever. */
static void probe_drain(bb_task_t *task, void *userdata)
{
    (void)task;
    probe_t *p = userdata;
    char c;
    if (recv(p->fd, &c, 1, 0) == 1)
    {
        p->fired++;
    }
}

static void raise_nofile_limit(void)
{
#if !defined(_WIN32)
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur < rl.rlim_max)
    {
        rl.rlim_cur = rl.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rl);
    }
#endif
}

/* Makes up to `want` pairs; returns how many it got. */
static int make_pairs(pair_t *pairs, int want)
{
    int made = 0;

    for (; made < want; made++)
    {
        bb_socket_t fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        {
            break;
        }
        pairs[made].client = fds[0];
        pairs[made].server = fds[1];
    }

    return made;
}

static void close_pairs(pair_t *pairs, int n)
{
    for (int i = 0; i < n; i++)
    {
        bb_socket_close(pairs[i].client);
        bb_socket_close(pairs[i].server);
    }
}

static void make_readable(pair_t *pair)
{
    BB_ASSERT(send(pair->server, "x", 1, 0) == 1);
}

static bb_runtime_t *new_runtime(void)
{
    bb_runtime_t *rt = bb_runtime_create();
    BB_ASSERT(rt != NULL);
    bb_runtime_set_running(rt);

    /* Keeps bb_runtime_tick() from sleeping out the runtime's idle timeout. */
    BB_ASSERT(bb_runtime_set_interval(rt, 1, noop_timer, NULL) != NULL);
    return rt;
}

/* Ticks until done() is true or the budget runs out. */
typedef int (*done_fn)(void *ctx);

static void tick_until(bb_runtime_t *rt, done_fn done, void *ctx)
{
    for (int i = 0; i < 20000 && !done(ctx); i++)
    {
        bb_runtime_tick(rt);
    }
    BB_ASSERT(done(ctx));
}

typedef struct
{
    probe_t *probes;
    int n;
    int min_fired;
} all_fired_t;

static int all_fired(void *ctx)
{
    all_fired_t *a = ctx;
    for (int i = 0; i < a->n; i++)
    {
        if (a->probes[i].fired < a->min_fired)
        {
            return 0;
        }
    }
    return 1;
}

/* ======================================================================= */
/* Tests                                                                   */
/* ======================================================================= */

static void test_many_descriptors_through_several_rehashes(void)
{
    printf("\tTesting %d descriptors through several table rehashes...\n", MANY_PAIRS);

    raise_nofile_limit();

    pair_t *pairs = calloc(MANY_PAIRS, sizeof(*pairs));
    probe_t *probes = calloc(MANY_PAIRS, sizeof(*probes));
    bb_task_t **tasks = calloc(MANY_PAIRS, sizeof(*tasks));
    BB_ASSERT(pairs && probes && tasks);

    int n = make_pairs(pairs, MANY_PAIRS);
    if (n < 4 * BB_RUNTIME_WATCHER_BUCKETS_INITIAL)
    {
        printf("\t  (skipped: only %d socket pairs available)\n", n);
        close_pairs(pairs, n);
        free(pairs);
        free(probes);
        free(tasks);
        return;
    }

    bb_runtime_t *rt = new_runtime();

    for (int i = 0; i < n; i++)
    {
        probes[i].fd = pairs[i].client;
        tasks[i] = bb_runtime_watch_fd(rt, pairs[i].client, BB_EVENT_READ, BB_WATCH_ONESHOT, probe_fire, &probes[i]);
        BB_ASSERT(tasks[i] != NULL);
    }

    BB_ASSERT(rt->watcher_count == n);
    BB_ASSERT(rt->poller->count == n);
    BB_ASSERT(rt->watcher_bucket_count > (unsigned)BB_RUNTIME_WATCHER_BUCKETS_INITIAL);
    BB_ASSERT(rt->watcher_bucket_count >= (unsigned)n);
    BB_ASSERT(rt->poller->hbucket_count > (unsigned)BB_POLLER_INITIAL_BUCKETS);

    /* Every watcher is where the table says, and agrees with its task. */
    for (int i = 0; i < n; i++)
    {
        _bb_runtime_watcher_t *w = _bb_runtime_find_watcher_exact(rt, pairs[i].client, BB_EVENT_READ);
        BB_ASSERT(w != NULL);
        BB_ASSERT(w->fd == pairs[i].client);
        BB_ASSERT(w->task == tasks[i]);
        BB_ASSERT(tasks[i]->watcher == w);
        BB_ASSERT(_bb_poller_find(rt->poller, pairs[i].client) >= 0);
    }

    /* Nothing is registered that should not be. */
    BB_ASSERT(_bb_runtime_find_watcher_exact(rt, pairs[0].client, BB_EVENT_WRITE) == NULL);

    for (int i = 0; i < n; i++)
    {
        make_readable(&pairs[i]);
    }

    all_fired_t done = { probes, n, 1 };
    tick_until(rt, all_fired, &done);

    /* Each one-shot fired exactly once and removed itself everywhere. */
    for (int i = 0; i < n; i++)
    {
        BB_ASSERT(probes[i].fired == 1);
    }
    BB_ASSERT(rt->watcher_count == 0);
    BB_ASSERT(rt->poller->count == 0);

    bb_runtime_destroy(rt);
    close_pairs(pairs, n);
    free(pairs);
    free(probes);
    free(tasks);
}

static void test_one_descriptor_with_several_watchers(void)
{
    printf("\tTesting several watchers on one descriptor...\n");

    pair_t pair;
    BB_ASSERT(make_pairs(&pair, 1) == 1);
    bb_runtime_t *rt = new_runtime();

    probe_t reads = { pair.client, 0 };
    probe_t writes = { pair.client, 0 };

    bb_task_t *read_task = bb_runtime_watch_fd(rt, pair.client, BB_EVENT_READ, BB_WATCH_PERSISTENT, probe_drain, &reads);
    bb_task_t *write_task = bb_runtime_watch_fd(rt, pair.client, BB_EVENT_WRITE, BB_WATCH_PERSISTENT, probe_fire, &writes);
    BB_ASSERT(read_task && write_task);

    /* Two watchers, one poller entry carrying the union of their interest. */
    BB_ASSERT(rt->watcher_count == 2);
    BB_ASSERT(rt->poller->count == 1);
    int idx = _bb_poller_find(rt->poller, pair.client);
    BB_ASSERT(idx >= 0);
    BB_ASSERT(rt->poller->fds[idx].events == (BB_EVENT_READ | BB_EVENT_WRITE));

    _bb_runtime_watcher_t *rw = _bb_runtime_find_watcher_exact(rt, pair.client, BB_EVENT_READ);
    _bb_runtime_watcher_t *ww = _bb_runtime_find_watcher_exact(rt, pair.client, BB_EVENT_WRITE);
    BB_ASSERT(rw && ww && rw != ww);
    BB_ASSERT(rw->task == read_task && ww->task == write_task);

    /* A socketpair is always writable: the writer fires. Make it readable too. */
    make_readable(&pair);
    for (int i = 0; i < 50 && (reads.fired == 0 || writes.fired == 0); i++)
    {
        bb_runtime_tick(rt);
    }
    BB_ASSERT(reads.fired == 1);
    BB_ASSERT(writes.fired > 0);

    /* Cancelling one leaves the other, and narrows the poller's interest. */
    bb_runtime_cancel_task(rt, read_task);
    BB_ASSERT(rt->watcher_count == 1);
    BB_ASSERT(_bb_runtime_find_watcher_exact(rt, pair.client, BB_EVENT_READ) == NULL);
    BB_ASSERT(_bb_runtime_find_watcher_exact(rt, pair.client, BB_EVENT_WRITE) == ww);
    idx = _bb_poller_find(rt->poller, pair.client);
    BB_ASSERT(idx >= 0 && rt->poller->fds[idx].events == BB_EVENT_WRITE);

    int writes_before = writes.fired;
    bb_runtime_tick(rt);
    BB_ASSERT(writes.fired > writes_before);

    bb_runtime_cancel_task(rt, write_task);
    BB_ASSERT(rt->watcher_count == 0);
    BB_ASSERT(rt->poller->count == 0);
    BB_ASSERT(_bb_poller_find(rt->poller, pair.client) < 0);

    bb_runtime_tick(rt); /* lets the cancelled tasks be reclaimed */
    bb_runtime_destroy(rt);
    close_pairs(&pair, 1);
}

static void test_replacing_a_watcher(void)
{
    printf("\tTesting replacing the watcher for a descriptor and event mask...\n");

    pair_t pair;
    BB_ASSERT(make_pairs(&pair, 1) == 1);
    bb_runtime_t *rt = new_runtime();

    probe_t old_probe = { pair.client, 0 };
    probe_t new_probe = { pair.client, 0 };

    bb_task_t *old_task = bb_runtime_watch_fd(rt, pair.client, BB_EVENT_READ, BB_WATCH_PERSISTENT, probe_drain, &old_probe);
    BB_ASSERT(old_task != NULL);
    BB_ASSERT(rt->watcher_count == 1);

    bb_task_t *new_task = bb_runtime_watch_fd(rt, pair.client, BB_EVENT_READ, BB_WATCH_PERSISTENT, probe_drain, &new_probe);
    BB_ASSERT(new_task != NULL && new_task != old_task);

    /* Same watcher slot, now owned by the new task; the old one is gone. */
    BB_ASSERT(rt->watcher_count == 1);
    BB_ASSERT(rt->poller->count == 1);
    _bb_runtime_watcher_t *w = _bb_runtime_find_watcher_exact(rt, pair.client, BB_EVENT_READ);
    BB_ASSERT(w != NULL);
    BB_ASSERT(w->task == new_task);
    BB_ASSERT(new_task->watcher == w);
    BB_ASSERT(old_task->watcher == NULL);
    BB_ASSERT(bb_task_is_cancelled(old_task));

    /* The replacement must not have unregistered the descriptor. */
    make_readable(&pair);
    for (int i = 0; i < 50 && new_probe.fired == 0; i++)
    {
        bb_runtime_tick(rt);
    }
    BB_ASSERT(new_probe.fired == 1);
    BB_ASSERT(old_probe.fired == 0);

    bb_runtime_cancel_task(rt, new_task);
    BB_ASSERT(rt->watcher_count == 0);
    BB_ASSERT(rt->poller->count == 0);

    bb_runtime_tick(rt);
    bb_runtime_destroy(rt);
    close_pairs(&pair, 1);
}

static void test_oneshot_unlinks_itself_amid_neighbours(void)
{
    printf("\tTesting one-shot watchers unlinking mid-dispatch among neighbours...\n");

    raise_nofile_limit();

    pair_t pairs[MEDIUM_PAIRS];
    probe_t probes[MEDIUM_PAIRS];
    memset(probes, 0, sizeof(probes));

    int n = make_pairs(pairs, MEDIUM_PAIRS);
    BB_ASSERT(n >= 2 * BB_RUNTIME_WATCHER_BUCKETS_INITIAL);

    bb_runtime_t *rt = new_runtime();

    for (int i = 0; i < n; i++)
    {
        probes[i].fd = pairs[i].client;
        BB_ASSERT(bb_runtime_watch_fd(rt, pairs[i].client, BB_EVENT_READ, BB_WATCH_ONESHOT, probe_fire, &probes[i]) != NULL);
    }

    /* Wake every other one: the survivors are interleaved with the removed. */
    int woken = 0;
    for (int i = 0; i < n; i += 2)
    {
        make_readable(&pairs[i]);
        woken++;
    }

    for (int i = 0; i < 20000 && rt->watcher_count > n - woken; i++)
    {
        bb_runtime_tick(rt);
    }
    BB_ASSERT(rt->watcher_count == n - woken);

    for (int i = 0; i < n; i++)
    {
        _bb_runtime_watcher_t *w = _bb_runtime_find_watcher_exact(rt, pairs[i].client, BB_EVENT_READ);
        if (i % 2 == 0)
        {
            BB_ASSERT(probes[i].fired == 1);
            BB_ASSERT(w == NULL);
        }
        else
        {
            BB_ASSERT(probes[i].fired == 0);
            BB_ASSERT(w != NULL && w->fd == pairs[i].client);
        }
    }

    /* The survivors still work. */
    for (int i = 1; i < n; i += 2)
    {
        make_readable(&pairs[i]);
    }
    all_fired_t done = { probes, n, 1 };
    tick_until(rt, all_fired, &done);
    BB_ASSERT(rt->watcher_count == 0);
    BB_ASSERT(rt->poller->count == 0);

    bb_runtime_destroy(rt);
    close_pairs(pairs, n);
}

static void test_unwatch_fd_touches_only_that_descriptor(void)
{
    printf("\tTesting unwatch_fd leaves other descriptors alone...\n");

    raise_nofile_limit();

    pair_t pairs[MEDIUM_PAIRS];
    probe_t probes[MEDIUM_PAIRS];
    memset(probes, 0, sizeof(probes));

    int n = make_pairs(pairs, MEDIUM_PAIRS);
    BB_ASSERT(n >= 2 * BB_RUNTIME_WATCHER_BUCKETS_INITIAL);

    bb_runtime_t *rt = new_runtime();

    for (int i = 0; i < n; i++)
    {
        probes[i].fd = pairs[i].client;
        BB_ASSERT(bb_runtime_watch_fd(rt, pairs[i].client, BB_EVENT_READ, BB_WATCH_PERSISTENT, probe_drain, &probes[i]) != NULL);
    }

    int removed = 0;
    for (int i = 0; i < n; i += 3)
    {
        BB_ASSERT(bb_runtime_unwatch_fd(rt, pairs[i].client) == 0);
        BB_ASSERT(bb_runtime_unwatch_fd(rt, pairs[i].client) == -1); /* nothing left on it */
        removed++;
    }

    BB_ASSERT(rt->watcher_count == n - removed);
    BB_ASSERT(rt->poller->count == n - removed);

    for (int i = 0; i < n; i++)
    {
        int gone = (i % 3 == 0);
        BB_ASSERT((_bb_runtime_find_watcher_exact(rt, pairs[i].client, BB_EVENT_READ) == NULL) == gone);
        BB_ASSERT((_bb_poller_find(rt->poller, pairs[i].client) < 0) == gone);
    }

    /* Everything is readable; only the watchers that remain fire. */
    for (int i = 0; i < n; i++)
    {
        make_readable(&pairs[i]);
    }

    int expected = n - removed;
    for (int i = 0; i < 20000; i++)
    {
        int fired = 0;
        for (int k = 0; k < n; k++)
        {
            fired += probes[k].fired > 0;
        }
        if (fired == expected)
        {
            break;
        }
        bb_runtime_tick(rt);
    }

    for (int i = 0; i < n; i++)
    {
        if (i % 3 == 0)
        {
            BB_ASSERT(probes[i].fired == 0);
        }
        else
        {
            BB_ASSERT(probes[i].fired >= 1);
        }
    }

    bb_runtime_tick(rt);
    bb_runtime_destroy(rt);
    close_pairs(pairs, n);
}

typedef struct
{
    bb_runtime_t *rt;
    bb_socket_t other; /* watchers to drop from inside this one's cleanup */
    int cleanups;
} reentrant_t;

static void noop_run(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

static void reentrant_cleanup(bb_task_t *task, void *userdata, bb_task_result_t result)
{
    (void)task;
    (void)result;
    reentrant_t *r = userdata;

    r->cleanups++;
    if (!bb_socket_is_invalid(r->other))
    {
        bb_socket_t other = r->other;
        r->other = BB_INVALID_SOCKET;
        bb_runtime_unwatch_fd(r->rt, other);
    }
}

static void watch_reentrant(bb_runtime_t *rt, bb_socket_t fd, reentrant_t *r)
{
    BB_ASSERT(bb_runtime_watch_fd_ex(rt, fd, BB_EVENT_READ, BB_WATCH_PERSISTENT,
                                     &(bb_task_config_t){ .run = noop_run, .userdata = r, .cleanup = reentrant_cleanup }) != NULL);
}

static void test_cleanup_callbacks_may_reenter_the_runtime(void)
{
    printf("\tTesting cleanup callbacks that call back into the runtime...\n");

    pair_t pairs[2];
    BB_ASSERT(make_pairs(pairs, 2) == 2);

    /* unwatch_fd on A runs A's cleanup, which unwatches B, while A's removal
     * is still in progress. */
    {
        bb_runtime_t *rt = new_runtime();
        reentrant_t a = { rt, pairs[1].client, 0 };
        reentrant_t b = { rt, BB_INVALID_SOCKET, 0 };

        watch_reentrant(rt, pairs[0].client, &a);
        watch_reentrant(rt, pairs[1].client, &b);
        BB_ASSERT(rt->watcher_count == 2);

        BB_ASSERT(bb_runtime_unwatch_fd(rt, pairs[0].client) == 0);
        BB_ASSERT(a.cleanups == 1);
        BB_ASSERT(b.cleanups == 1);
        BB_ASSERT(rt->watcher_count == 0);
        BB_ASSERT(rt->poller->count == 0);

        bb_runtime_tick(rt);
        bb_runtime_destroy(rt);
    }

    /* The same during bb_runtime_destroy(), whichever of the two it reaches
     * first: each is cleaned up exactly once, with the table consistent. */
    {
        bb_runtime_t *rt = new_runtime();
        reentrant_t a = { rt, pairs[1].client, 0 };
        reentrant_t b = { rt, BB_INVALID_SOCKET, 0 };

        watch_reentrant(rt, pairs[0].client, &a);
        watch_reentrant(rt, pairs[1].client, &b);

        bb_runtime_destroy(rt);
        BB_ASSERT(a.cleanups == 1);
        BB_ASSERT(b.cleanups == 1);
    }

    close_pairs(pairs, 2);
}

static void test_table_survives_register_cancel_reuse(void)
{
    printf("\tTesting released watcher nodes are reused safely...\n");

    raise_nofile_limit();

    pair_t pairs[MEDIUM_PAIRS];
    probe_t probes[MEDIUM_PAIRS];
    memset(probes, 0, sizeof(probes));

    int n = make_pairs(pairs, MEDIUM_PAIRS);
    BB_ASSERT(n >= 2 * BB_RUNTIME_WATCHER_BUCKETS_INITIAL);

    bb_runtime_t *rt = new_runtime();
    bb_task_t *tasks[MEDIUM_PAIRS];

    /* Rounds of "watch everything, cancel everything, in a different order":
     * the free list hands the same nodes back out in a new arrangement. */
    for (int round = 0; round < 6; round++)
    {
        for (int i = 0; i < n; i++)
        {
            probes[i].fd = pairs[i].client;
            tasks[i] = bb_runtime_watch_fd(rt, pairs[i].client, BB_EVENT_READ, BB_WATCH_PERSISTENT, probe_drain, &probes[i]);
            BB_ASSERT(tasks[i] != NULL);
        }
        BB_ASSERT(rt->watcher_count == n);

        for (int i = 0; i < n; i++)
        {
            int k = (round % 2 == 0) ? i : n - 1 - i;
            bb_runtime_cancel_task(rt, tasks[k]);
        }
        BB_ASSERT(rt->watcher_count == 0);
        BB_ASSERT(rt->poller->count == 0);

        bb_runtime_tick(rt);
    }

    /* And it still works after all that. */
    probes[7].fd = pairs[7].client;
    bb_task_t *last = bb_runtime_watch_fd(rt, pairs[7].client, BB_EVENT_READ, BB_WATCH_PERSISTENT, probe_drain, &probes[7]);
    BB_ASSERT(last != NULL);
    make_readable(&pairs[7]);
    for (int i = 0; i < 50 && probes[7].fired == 0; i++)
    {
        bb_runtime_tick(rt);
    }
    BB_ASSERT(probes[7].fired >= 1);

    bb_runtime_destroy(rt);
    close_pairs(pairs, n);
}

int main(void)
{
    printf("Running runtime fd table tests...\n");

    test_many_descriptors_through_several_rehashes();
    test_one_descriptor_with_several_watchers();
    test_replacing_a_watcher();
    test_oneshot_unlinks_itself_amid_neighbours();
    test_unwatch_fd_touches_only_that_descriptor();
    test_cleanup_callbacks_may_reenter_the_runtime();
    test_table_survives_register_cancel_reuse();

    printf("Runtime fd table tests passed.\n");
    return 0;
}
