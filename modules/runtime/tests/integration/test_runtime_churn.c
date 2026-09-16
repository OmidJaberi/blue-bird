#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "blue-bird/runtime/runtime.h"
#include "runtime_internal.h"

/* struct bb_poller is opaque in poller.h; the churn assertions below
 * read poller->count/capacity directly to catch bookkeeping drift that
 * callback-firing counts alone would hide. */
#include "poller_backend.h"

/*
 * Event-loop stress tests: high concurrent connection churn
 * ========================================================
 *
 * The functional watch/unwatch tests in test_runtime.c all operate on a
 * single fd at a time, which never exercises the parts of the runtime
 * that only misbehave at scale:
 *
 *   - watchers[] and the poller's fds[] both start at capacity 16 and
 *     grow by doubling (realloc). Nothing below 17 concurrent fds ever
 *     triggers a growth, so a stale-pointer-after-realloc bug or an
 *     off-by-one in the grow path is invisible to those tests.
 *
 *   - Both arrays are compacted by *swap-remove* (move the last entry
 *     into the freed slot). With one watcher that's a no-op; with
 *     hundreds being added and removed in arbitrary order it's the most
 *     likely place for a watcher to be skipped, dropped, or dispatched
 *     to the wrong task.
 *
 *   - BB_RUNTIME_MAX_EVENTS caps a single bb_poller_wait() at 64 ready
 *     fds. Above that, readiness necessarily spans multiple ticks, and
 *     the loop must drain the remainder rather than losing it.
 *
 *   - Descriptor numbers are recycled aggressively by the OS. A
 *     connection closed this tick hands its fd number to a connection
 *     accepted the next tick, so any bookkeeping keyed on the raw fd
 *     must be fully cleared on unwatch.
 *
 * These tests drive real loopback sockets (not pipes) so the kernel
 * participates exactly as it would for accepted connections, and assert
 * on runtime->watcher_count / poller->count directly to catch
 * bookkeeping drift that firing counts alone would hide.
 */

/* Comfortably past both arrays' initial capacity of 16 and past
 * BB_RUNTIME_MAX_EVENTS (64), so growth and multi-tick drain are both
 * forced. Each pair costs two descriptors. */
#define CHURN_PAIRS 100

/* Rounds of full add/fire/remove cycles in the repeated-churn test. */
#define CHURN_ROUNDS 8

typedef struct {
    bb_socket_t client;
    bb_socket_t server;
} churn_pair_t;

/* ======================================================================= */
/* Helpers                                                                 */
/* ======================================================================= */

/*
 * Creates one connected loopback TCP pair. Real sockets rather than
 * pipes: accepted connections are what the runtime actually watches in
 * production, and socket fds go through the same kernel readiness paths
 * (including EPOLLHUP/EV_EOF on peer close) that pipes only approximate.
 */
static int _make_pair(churn_pair_t *pair)
{
    bb_socket_t listener = socket(AF_INET, SOCK_STREAM, 0);

    if (bb_socket_is_invalid(listener))
    {
        return -1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(listener, 1) != 0)
    {
        bb_socket_close(listener);
        return -1;
    }

    socklen_t addr_len = sizeof(addr);

    if (getsockname(listener, (struct sockaddr *)&addr, &addr_len) != 0)
    {
        bb_socket_close(listener);
        return -1;
    }

    bb_socket_t client = socket(AF_INET, SOCK_STREAM, 0);

    if (bb_socket_is_invalid(client))
    {
        bb_socket_close(listener);
        return -1;
    }

    if (connect(client, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        bb_socket_close(client);
        bb_socket_close(listener);
        return -1;
    }

    bb_socket_t server = accept(listener, NULL, NULL);

    bb_socket_close(listener); // no longer needed; the pair is established

    if (bb_socket_is_invalid(server))
    {
        bb_socket_close(client);
        return -1;
    }

    pair->client = client;
    pair->server = server;

    return 0;
}

static void _close_pair(churn_pair_t *pair)
{
    if (!bb_socket_is_invalid(pair->server))
    {
        bb_socket_close(pair->server);
        pair->server = BB_INVALID_SOCKET;
    }
    if (!bb_socket_is_invalid(pair->client))
    {
        bb_socket_close(pair->client);
        pair->client = BB_INVALID_SOCKET;
    }
}

/*
 * Opens as many pairs as the process's descriptor budget allows, up to
 * `wanted`. Returns how many were actually created -- the caller scales
 * its assertions to that, so a tight RLIMIT_NOFILE on the CI box
 * weakens the stress test rather than failing it spuriously.
 */
static int _make_pairs(churn_pair_t *pairs, int wanted)
{
    int made = 0;

    for (int i = 0; i < wanted; i++)
    {
        if (_make_pair(&pairs[i]) != 0)
        {
            break;
        }
        made++;
    }

    return made;
}

static void _close_pairs(churn_pair_t *pairs, int count)
{
    for (int i = 0; i < count; i++)
    {
        _close_pair(&pairs[i]);
    }
}

/* Drains one byte so a server fd stops being readable, letting a
 * persistent watcher settle instead of re-firing every tick. */
static void _drain_one(bb_socket_t fd)
{
    char sink[1];
    (void)recv(fd, sink, sizeof(sink), 0);
}

/*
 * Ticks a fixed number of times *without* blocking for the full idle
 * timeout on each one.
 *
 * With nothing watched, no timers, and an empty scheduler,
 * bb_runtime_tick() has nothing to wake it, so it blocks in
 * bb_poller_wait() for BB_RUNTIME_IDLE_TIMEOUT_MS (1s). Tests that tick
 * a fixed budget to prove a *negative* -- "nothing fires from here on"
 * -- spend that second on every tick after the interesting work is
 * done, which turns a 100-tick budget into 100 seconds of dead waiting.
 *
 * Keeping one repeating timer armed gives _bb_runtime_next_timeout_ms()
 * something permanently due, so each wait returns promptly. A 0ms
 * interval is safe: the runtime re-arms a non-advancing repeat to
 * now + 1 rather than spinning. The timer's callback is deliberately
 * separate from churn_watch_cb() so it can't perturb the fire counts
 * being asserted on.
 */
static void _quiet_tick_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

static void _tick_quiet(bb_runtime_t *runtime, int ticks)
{
    bb_task_t *keepalive = bb_runtime_set_interval(runtime, 0, _quiet_tick_cb, NULL);
    BB_ASSERT(keepalive != NULL);

    for (int i = 0; i < ticks; i++)
    {
        bb_runtime_tick(runtime);
    }

    BB_ASSERT(bb_runtime_cancel_task(runtime, keepalive) == 0);

    // Let the cancelled keepalive drain so it can't outlive this call.
    bb_runtime_tick(runtime);
}

/*
 * Ticks until `predicate` holds or the budget runs out. Returns the
 * number of ticks spent; the caller asserts the predicate separately so
 * a failure reports the real condition rather than "ran out of ticks".
 */
static int _tick_until(bb_runtime_t *runtime, int (*predicate)(void *), void *ctx, int max_ticks)
{
    /* Same reason as _tick_quiet(): without this, a predicate that never
     * becomes true turns the budget into max_ticks seconds of blocking
     * before the assertion finally reports the failure. */
    bb_task_t *keepalive = bb_runtime_set_interval(runtime, 0, _quiet_tick_cb, NULL);
    BB_ASSERT(keepalive != NULL);

    int ticks = 0;

    while (ticks < max_ticks && !predicate(ctx))
    {
        bb_runtime_tick(runtime);
        ticks++;
    }

    BB_ASSERT(bb_runtime_cancel_task(runtime, keepalive) == 0);
    bb_runtime_tick(runtime);

    return ticks;
}

/* ======================================================================= */
/* Per-watcher fire accounting                                             */
/* ======================================================================= */

/*
 * Each watcher gets its own slot, so we can assert not just "the right
 * number of callbacks ran" but "every individual watcher ran, and none
 * ran on another's behalf" -- the failure mode a swap-remove bug
 * actually produces.
 */
static int churn_fire_count[CHURN_PAIRS];

static void _reset_fire_counts(void)
{
    memset(churn_fire_count, 0, sizeof(churn_fire_count));
}

static void churn_watch_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    int index = (int)(intptr_t)userdata;

    BB_ASSERT(index >= 0 && index < CHURN_PAIRS);

    churn_fire_count[index]++;
}

typedef struct {
    int count;
} churn_expect_t;

static int _all_fired(void *ctx)
{
    churn_expect_t *expect = ctx;

    for (int i = 0; i < expect->count; i++)
    {
        if (churn_fire_count[i] == 0)
        {
            return 0;
        }
    }

    return 1;
}

/* ======================================================================= */
/* Tests                                                                   */
/* ======================================================================= */

/*
 * Many fds watched at once, all readable simultaneously.
 *
 * Forces watchers[] and the poller's fds[] well past their initial
 * capacity of 16 (several realloc doublings each), and puts more ready
 * fds in flight than BB_RUNTIME_MAX_EVENTS can report in a single
 * bb_poller_wait(). Every watcher must still fire: none may be lost to
 * the event-batch cap, and none may be left stranded by a realloc that
 * moved the arrays out from under an in-flight pointer.
 */
static void test_churn_many_concurrent_watchers_all_fire(void)
{
    printf("\tRunning test_churn_many_concurrent_watchers_all_fire...\n");

    _reset_fire_counts();

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    churn_pair_t *pairs = calloc(CHURN_PAIRS, sizeof(*pairs));
    BB_ASSERT(pairs != NULL);

    int count = _make_pairs(pairs, CHURN_PAIRS);

    // Below this there aren't enough fds to force even one array growth,
    // so the test would pass without testing anything.
    if (count <= BB_RUNTIME_WATCHERS_INITIAL_CAPACITY)
    {
        printf("\t\tskipped: only %d socket pairs available (need > %d)\n",
               count, BB_RUNTIME_WATCHERS_INITIAL_CAPACITY);
        _close_pairs(pairs, count);
        free(pairs);
        bb_runtime_destroy(runtime);
        bb_platform_net_cleanup();
        return;
    }

    for (int i = 0; i < count; i++)
    {
        bb_task_t *task = bb_runtime_watch_fd(runtime, pairs[i].server, BB_EVENT_READ,
                                              BB_WATCH_ONESHOT, churn_watch_cb,
                                              (void *)(intptr_t)i);
        BB_ASSERT(task != NULL);

        // Make every server fd readable and leave the byte unread.
        BB_ASSERT(send(pairs[i].client, "x", 1, 0) == 1);
    }

    // Both arrays must have grown past their initial capacity to hold
    // this many entries -- i.e. the realloc path really was exercised.
    BB_ASSERT(runtime->watcher_count == count);
    BB_ASSERT(runtime->watcher_capacity > BB_RUNTIME_WATCHERS_INITIAL_CAPACITY);
    BB_ASSERT(runtime->poller->count == count);
    BB_ASSERT(runtime->poller->capacity > BB_POLLER_INITIAL_CAPACITY);

    runtime->running = true;

    churn_expect_t expect = { .count = count };

    // Two ticks per watcher is a generous budget: readiness for all
    // `count` fds needs ceil(count / BB_RUNTIME_MAX_EVENTS) waits, plus
    // a tick each to run the scheduled tasks.
    _tick_until(runtime, _all_fired, &expect, count * 2 + 16);

    for (int i = 0; i < count; i++)
    {
        // Every watcher fired, and each fired exactly once -- oneshot
        // semantics must survive the swap-remove churn of hundreds of
        // watchers being torn down mid-dispatch.
        BB_ASSERT(churn_fire_count[i] == 1);
    }

    // Oneshot watchers deregister themselves as they fire, so both the
    // runtime's and the poller's bookkeeping must be back to empty.
    BB_ASSERT(runtime->watcher_count == 0);
    BB_ASSERT(runtime->poller->count == 0);

    _close_pairs(pairs, count);
    free(pairs);

    bb_runtime_destroy(runtime);
    bb_platform_net_cleanup();
}

/*
 * Repeated add -> fire -> remove rounds over the same descriptors.
 *
 * This is the shape a busy server actually produces: connections arrive
 * and depart continuously while the watcher table stays roughly the
 * same size. Each round re-registers every fd from scratch, so any
 * entry the previous round failed to clear out of watchers[] or the
 * poller's fds[] accumulates -- and the exact-count assertions at the
 * top of each round catch that drift on the very next iteration rather
 * than letting it silently pile up.
 */
static void test_churn_repeated_watch_unwatch_rounds(void)
{
    printf("\tRunning test_churn_repeated_watch_unwatch_rounds...\n");

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    churn_pair_t *pairs = calloc(CHURN_PAIRS, sizeof(*pairs));
    BB_ASSERT(pairs != NULL);

    int count = _make_pairs(pairs, CHURN_PAIRS);

    if (count <= BB_RUNTIME_WATCHERS_INITIAL_CAPACITY)
    {
        printf("\t\tskipped: only %d socket pairs available (need > %d)\n",
               count, BB_RUNTIME_WATCHERS_INITIAL_CAPACITY);
        _close_pairs(pairs, count);
        free(pairs);
        bb_runtime_destroy(runtime);
        bb_platform_net_cleanup();
        return;
    }

    runtime->running = true;

    for (int round = 0; round < CHURN_ROUNDS; round++)
    {
        _reset_fire_counts();

        // Every round must start from a genuinely clean slate. If the
        // previous round leaked a watcher or a poller entry, this fails
        // immediately and points at the round that leaked.
        BB_ASSERT(runtime->watcher_count == 0);
        BB_ASSERT(runtime->poller->count == 0);

        for (int i = 0; i < count; i++)
        {
            bb_task_t *task = bb_runtime_watch_fd(runtime, pairs[i].server, BB_EVENT_READ,
                                                  BB_WATCH_PERSISTENT, churn_watch_cb,
                                                  (void *)(intptr_t)i);
            BB_ASSERT(task != NULL);

            BB_ASSERT(send(pairs[i].client, "x", 1, 0) == 1);
        }

        BB_ASSERT(runtime->watcher_count == count);
        BB_ASSERT(runtime->poller->count == count);

        churn_expect_t expect = { .count = count };

        _tick_until(runtime, _all_fired, &expect, count * 2 + 16);

        for (int i = 0; i < count; i++)
        {
            BB_ASSERT(churn_fire_count[i] > 0);
        }

        /*
         * Tear down in reverse order. Forward order happens to remove
         * the swap-remove "hot" slot last; reverse order repeatedly
         * removes the entry that a previous swap just relocated, which
         * is where index-tracking bugs surface.
         */
        for (int i = count - 1; i >= 0; i--)
        {
            _drain_one(pairs[i].server); // stop it being readable
            BB_ASSERT(bb_runtime_unwatch_fd(runtime, pairs[i].server) == 0);
        }

        BB_ASSERT(runtime->watcher_count == 0);
        BB_ASSERT(runtime->poller->count == 0);

        // Let the cancelled tasks that unwatch_fd() queued for
        // destruction actually drain, so they don't accumulate across
        // rounds in the scheduler.
        _tick_quiet(runtime, 1);
    }

    _close_pairs(pairs, count);
    free(pairs);

    bb_runtime_destroy(runtime);
    bb_platform_net_cleanup();
}

/*
 * Half the watchers removed while the other half are mid-dispatch.
 *
 * _bb_runtime_wait() walks watchers[] while dispatching, and a oneshot
 * watcher removes itself from that same array during the walk by
 * swapping the last entry into its slot. Removing a large, interleaved
 * subset up front means the surviving watchers are exactly the ones
 * most likely to be relocated by those swaps. Every survivor must still
 * fire, and every removed watcher must stay silent.
 */
static void test_churn_interleaved_removal_preserves_survivors(void)
{
    printf("\tRunning test_churn_interleaved_removal_preserves_survivors...\n");

    _reset_fire_counts();

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    churn_pair_t *pairs = calloc(CHURN_PAIRS, sizeof(*pairs));
    BB_ASSERT(pairs != NULL);

    int count = _make_pairs(pairs, CHURN_PAIRS);

    if (count <= BB_RUNTIME_WATCHERS_INITIAL_CAPACITY)
    {
        printf("\t\tskipped: only %d socket pairs available (need > %d)\n",
               count, BB_RUNTIME_WATCHERS_INITIAL_CAPACITY);
        _close_pairs(pairs, count);
        free(pairs);
        bb_runtime_destroy(runtime);
        bb_platform_net_cleanup();
        return;
    }

    for (int i = 0; i < count; i++)
    {
        BB_ASSERT(bb_runtime_watch_fd(runtime, pairs[i].server, BB_EVENT_READ,
                                      BB_WATCH_ONESHOT, churn_watch_cb,
                                      (void *)(intptr_t)i) != NULL);

        BB_ASSERT(send(pairs[i].client, "x", 1, 0) == 1);
    }

    BB_ASSERT(runtime->watcher_count == count);

    // Drop every even-indexed watcher, leaving a maximally fragmented
    // table for the dispatch walk to traverse.
    int removed = 0;

    for (int i = 0; i < count; i += 2)
    {
        BB_ASSERT(bb_runtime_unwatch_fd(runtime, pairs[i].server) == 0);
        removed++;
    }

    int survivors = count - removed;

    BB_ASSERT(runtime->watcher_count == survivors);
    BB_ASSERT(runtime->poller->count == survivors);

    runtime->running = true;

    // Ticks a fixed budget rather than stopping at "all fired": the
    // point is partly to confirm the removed watchers stay silent even
    // after the survivors are done, so the loop must keep running past
    // that point.
    _tick_quiet(runtime, survivors * 2 + 16);

    for (int i = 0; i < count; i++)
    {
        if (i % 2 == 0)
        {
            // Unwatched before the loop ever ran: must never fire, even
            // though its fd stayed readable the whole time.
            BB_ASSERT(churn_fire_count[i] == 0);
        }
        else
        {
            BB_ASSERT(churn_fire_count[i] == 1);
        }
    }

    BB_ASSERT(runtime->watcher_count == 0);
    BB_ASSERT(runtime->poller->count == 0);

    _close_pairs(pairs, count);
    free(pairs);

    bb_runtime_destroy(runtime);
    bb_platform_net_cleanup();
}

/*
 * Descriptor-number recycling across churn.
 *
 * The OS hands out the lowest free descriptor, so under churn a closed
 * connection's fd number is immediately reused by the next accepted
 * one. Both watchers[] and the poller's fds[] are keyed on that raw
 * number, so if unwatch leaves anything behind, the *new* connection
 * inherits the old one's registration: _bb_runtime_fd_registered_mask()
 * reports the fd as already registered and skips the poller registration
 * entirely, and the new watcher silently never fires.
 *
 * This closes and reopens repeatedly to make number reuse near-certain,
 * and asserts the fresh watcher fires on every generation.
 */
static void test_churn_recycled_fd_numbers_rewatch_cleanly(void)
{
    printf("\tRunning test_churn_recycled_fd_numbers_rewatch_cleanly...\n");

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    runtime->running = true;

    int reuse_observed = 0;
    bb_socket_t previous_fd = BB_INVALID_SOCKET;

    for (int generation = 0; generation < 32; generation++)
    {
        _reset_fire_counts();

        churn_pair_t pair;

        if (_make_pair(&pair) != 0)
        {
            printf("\t\tskipped: could not create socket pair at generation %d\n", generation);
            break;
        }

        if (pair.server == previous_fd)
        {
            reuse_observed++;
        }

        BB_ASSERT(bb_runtime_watch_fd(runtime, pair.server, BB_EVENT_READ,
                                      BB_WATCH_ONESHOT, churn_watch_cb,
                                      (void *)(intptr_t)0) != NULL);

        BB_ASSERT(send(pair.client, "x", 1, 0) == 1);

        churn_expect_t expect = { .count = 1 };

        _tick_until(runtime, _all_fired, &expect, 32);

        // The crux: a watcher on a *recycled* fd number must fire just
        // like one on a never-before-seen number. A stale entry left by
        // the previous generation would make this zero.
        BB_ASSERT(churn_fire_count[0] == 1);

        BB_ASSERT(runtime->watcher_count == 0);
        BB_ASSERT(runtime->poller->count == 0);

        previous_fd = pair.server;

        _close_pair(&pair);

        // Drain the destroyed oneshot task before the next generation.
        _tick_quiet(runtime, 1);
    }

    // Not an assertion about the runtime -- just confirmation that the
    // scenario above actually exercised fd reuse rather than getting a
    // fresh number every time (which would make the test vacuous).
    printf("\t\tfd number reuse observed in %d generation(s)\n", reuse_observed);

    bb_runtime_destroy(runtime);
    bb_platform_net_cleanup();
}

/*
 * Churn with no tick in between: watchers added and removed faster than
 * the loop runs.
 *
 * A burst of connections that all arrive and disconnect within a single
 * loop iteration never gives the poller a chance to report on them. The
 * bookkeeping must still net out to zero, and -- critically -- the next
 * tick must not dispatch anything for the fds that came and went.
 */
static void test_churn_add_remove_without_intervening_ticks(void)
{
    printf("\tRunning test_churn_add_remove_without_intervening_ticks...\n");

    _reset_fire_counts();

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    churn_pair_t *pairs = calloc(CHURN_PAIRS, sizeof(*pairs));
    BB_ASSERT(pairs != NULL);

    int count = _make_pairs(pairs, CHURN_PAIRS);

    if (count <= BB_RUNTIME_WATCHERS_INITIAL_CAPACITY)
    {
        printf("\t\tskipped: only %d socket pairs available (need > %d)\n",
               count, BB_RUNTIME_WATCHERS_INITIAL_CAPACITY);
        _close_pairs(pairs, count);
        free(pairs);
        bb_runtime_destroy(runtime);
        bb_platform_net_cleanup();
        return;
    }

    // Watch and immediately unwatch, one fd at a time, with the fd made
    // readable in between -- so readiness genuinely exists but the loop
    // never gets a chance to observe it.
    for (int i = 0; i < count; i++)
    {
        BB_ASSERT(bb_runtime_watch_fd(runtime, pairs[i].server, BB_EVENT_READ,
                                      BB_WATCH_PERSISTENT, churn_watch_cb,
                                      (void *)(intptr_t)i) != NULL);

        BB_ASSERT(send(pairs[i].client, "x", 1, 0) == 1);

        BB_ASSERT(bb_runtime_unwatch_fd(runtime, pairs[i].server) == 0);
    }

    BB_ASSERT(runtime->watcher_count == 0);
    BB_ASSERT(runtime->poller->count == 0);

    runtime->running = true;

    _tick_quiet(runtime, 8);

    // Every fd is still readable, but nothing is watching any of them.
    for (int i = 0; i < count; i++)
    {
        BB_ASSERT(churn_fire_count[i] == 0);
    }

    _close_pairs(pairs, count);
    free(pairs);

    bb_runtime_destroy(runtime);
    bb_platform_net_cleanup();
}

/*
 * Teardown with a large watcher table still fully populated.
 *
 * The normal path drains watchers as they fire; this is the abrupt
 * shutdown case, where bb_runtime_destroy() has to unregister and free
 * hundreds of live watchers at once. It must not crash, double-free, or
 * leak -- the assertion here is "runs to completion cleanly", with
 * ASan/valgrind builds supplying the rest.
 */
static void test_churn_destroy_with_many_live_watchers(void)
{
    printf("\tRunning test_churn_destroy_with_many_live_watchers...\n");

    _reset_fire_counts();

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    churn_pair_t *pairs = calloc(CHURN_PAIRS, sizeof(*pairs));
    BB_ASSERT(pairs != NULL);

    int count = _make_pairs(pairs, CHURN_PAIRS);

    if (count <= 0)
    {
        printf("\t\tskipped: no socket pairs available\n");
        free(pairs);
        bb_runtime_destroy(runtime);
        bb_platform_net_cleanup();
        return;
    }

    for (int i = 0; i < count; i++)
    {
        BB_ASSERT(bb_runtime_watch_fd(runtime, pairs[i].server, BB_EVENT_READ,
                                      BB_WATCH_PERSISTENT, churn_watch_cb,
                                      (void *)(intptr_t)i) != NULL);
    }

    // Also leave timers and queued tasks outstanding, so destroy has to
    // tear down all three subsystems at once rather than just watchers.
    for (int i = 0; i < 32; i++)
    {
        BB_ASSERT(bb_runtime_set_timeout(runtime, 60000, churn_watch_cb, (void *)(intptr_t)0) != NULL);
        BB_ASSERT(bb_runtime_schedule(runtime, churn_watch_cb, (void *)(intptr_t)0) != NULL);
    }

    BB_ASSERT(runtime->watcher_count == count);

    // Destroy without ever running the loop: nothing was dispatched, so
    // every watcher, timer, and queued task is still live.
    bb_runtime_destroy(runtime);

    _close_pairs(pairs, count);
    free(pairs);

    bb_platform_net_cleanup();
}

int main(void)
{
    printf("Starting runtime connection-churn stress test...\n");

    test_churn_many_concurrent_watchers_all_fire();
    test_churn_repeated_watch_unwatch_rounds();
    test_churn_interleaved_removal_preserves_survivors();
    test_churn_recycled_fd_numbers_rewatch_cleanly();
    test_churn_add_remove_without_intervening_ticks();
    test_churn_destroy_with_many_live_watchers();

    printf("Runtime connection-churn stress test passed.\n");
    return 0;
}
