#include <stdint.h>
#include <stdlib.h>

#include "blue-bird/utils/time.h"
#include "blue-bird/utils/platform.h"

#include "runtime_internal.h"


static bb_runtime_t *g_runtime = NULL;

bb_runtime_t *bb_runtime_default(void)
{
    if (!g_runtime)
    {
        g_runtime = bb_runtime_create();
    }
    return g_runtime;
}

/* ======================================================================= */
/* fd watcher table                                                        */
/*                                                                         */
/* A hash table keyed by fd (see _bb_runtime_watcher_t). Everything that   */
/* used to scan every watcher -- dispatching an event, working out what    */
/* the poller must be registered for, replacing a watch, cancelling a      */
/* task -- now touches only the chain for one fd, or none at all.          */
/* ======================================================================= */

static inline unsigned _bb_runtime_bucket_for(unsigned bucket_count, bb_socket_t fd)
{
    /* Multiplicative (Fibonacci) hashing: spreads both the small consecutive
     * descriptors of POSIX and the large, patterned SOCKET values of Windows. */
    uint64_t h = (uint64_t)(uintptr_t)fd * 0x9E3779B97F4A7C15ull;
    return (unsigned)(h >> 32) & (bucket_count - 1);
}

static void _bb_runtime_chain_append(_bb_runtime_watcher_t **head, _bb_runtime_watcher_t *node)
{
    /* Appended, not prepended, so watchers on one fd are always visited in the
     * order they were registered. */
    _bb_runtime_watcher_t **link = head;

    while (*link)
    {
        link = &(*link)->chain_next;
    }

    node->chain_next = NULL;
    node->chain_prev = link;
    *link = node;
}

/* Doubles the table once there are more watchers than buckets, keeping chains
 * short. Best effort: if the memory is not there the old, merely more crowded,
 * table keeps working. */
static void _bb_runtime_watchers_grow(bb_runtime_t *runtime)
{
    unsigned new_count = runtime->watcher_bucket_count * 2;
    _bb_runtime_watcher_t **grown = calloc(new_count, sizeof(*grown));

    if (!grown)
    {
        return;
    }

    for (unsigned b = 0; b < runtime->watcher_bucket_count; b++)
    {
        _bb_runtime_watcher_t *node = runtime->watcher_buckets[b];

        while (node)
        {
            _bb_runtime_watcher_t *next = node->chain_next;
            _bb_runtime_chain_append(&grown[_bb_runtime_bucket_for(new_count, node->fd)], node);
            node = next;
        }
    }

    free(runtime->watcher_buckets);
    runtime->watcher_buckets = grown;
    runtime->watcher_bucket_count = new_count;
}

static _bb_runtime_watcher_t *_bb_runtime_watcher_acquire(bb_runtime_t *runtime)
{
    _bb_runtime_watcher_t *node = runtime->free_watchers;

    if (node)
    {
        runtime->free_watchers = node->chain_next;
        return node;
    }

    return malloc(sizeof(*node));
}

static void _bb_runtime_watcher_release(bb_runtime_t *runtime, _bb_runtime_watcher_t *node)
{
    node->chain_next = runtime->free_watchers;
    runtime->free_watchers = node;
}

static void _bb_runtime_watcher_link(bb_runtime_t *runtime, _bb_runtime_watcher_t *node)
{
    if ((unsigned)runtime->watcher_count >= runtime->watcher_bucket_count)
    {
        _bb_runtime_watchers_grow(runtime);
    }

    _bb_runtime_chain_append(&runtime->watcher_buckets[_bb_runtime_bucket_for(runtime->watcher_bucket_count, node->fd)], node);
    runtime->watcher_count++;
}

/* Takes the watcher out of the table and hands its node back for reuse. Does
 * not touch the poller. */
static void _bb_runtime_watcher_remove(bb_runtime_t *runtime, _bb_runtime_watcher_t *node)
{
    *node->chain_prev = node->chain_next;
    if (node->chain_next)
    {
        node->chain_next->chain_prev = node->chain_prev;
    }

    if (node->task && node->task->watcher == node)
    {
        node->task->watcher = NULL;
    }

    runtime->watcher_count--;
    _bb_runtime_watcher_release(runtime, node);
}

static _bb_runtime_watcher_t *_bb_runtime_fd_chain(bb_runtime_t *runtime, bb_socket_t fd)
{
    return runtime->watcher_buckets[_bb_runtime_bucket_for(runtime->watcher_bucket_count, fd)];
}

_bb_runtime_watcher_t *_bb_runtime_find_watcher_exact(bb_runtime_t *runtime, bb_socket_t fd, int events)
{
    for (_bb_runtime_watcher_t *w = _bb_runtime_fd_chain(runtime, fd); w; w = w->chain_next)
    {
        if (w->fd == fd && w->events == events)
        {
            return w;
        }
    }

    return NULL;
}

/* Union of the event masks everything watching fd has asked for. */
static int _bb_runtime_fd_registered_mask(bb_runtime_t *runtime, bb_socket_t fd)
{
    int mask = 0;

    for (_bb_runtime_watcher_t *w = _bb_runtime_fd_chain(runtime, fd); w; w = w->chain_next)
    {
        if (w->fd == fd)
        {
            mask |= w->events;
        }
    }

    return mask;
}

bb_runtime_t *bb_runtime_create(void)
{
    bb_platform_net_init();

    bb_runtime_t *runtime = calloc(1, sizeof(bb_runtime_t));

    if (!runtime)
    {
        return NULL;
    }

    runtime->scheduler = bb_scheduler_create();

    if (!runtime->scheduler)
    {
        free(runtime);
        return NULL;
    }

    runtime->poller = bb_poller_create();

    if (!runtime->poller)
    {
        bb_scheduler_destroy(runtime->scheduler);
        free(runtime);
        return NULL;
    }

    runtime->watcher_buckets = calloc(BB_RUNTIME_WATCHER_BUCKETS_INITIAL, sizeof(*runtime->watcher_buckets));

    if (!runtime->watcher_buckets)
    {
        bb_poller_destroy(runtime->poller);
        bb_scheduler_destroy(runtime->scheduler);
        free(runtime);
        return NULL;
    }

    runtime->watcher_bucket_count = BB_RUNTIME_WATCHER_BUCKETS_INITIAL;

    runtime->timers = bb_timer_heap_create();

    if (!runtime->timers)
    {
        free(runtime->watcher_buckets);
        bb_poller_destroy(runtime->poller);
        bb_scheduler_destroy(runtime->scheduler);
        free(runtime);
        return NULL;
    }

    runtime->running = false;

    return runtime;
}

void bb_runtime_destroy(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    // Clean-up scheduler:
    bb_task_t *task;
    while ((task = bb_scheduler_next(runtime->scheduler)))
    {
        task->state &= ~BB_TASK_SCHEDULED;

        // A persistent task (repeating timer / persistent watcher) can be
        // queued here AND still be owned by runtime->timers or
        // runtime->watchers. Those loops below destroy it, so destroying it
        // here too would be a double free.
        if (task->state & BB_TASK_PERSISTENT)
        {
            continue;
        }

        bb_task_destroy(task);
    }

    // Clean-up watchers
    // Anything still here was never (re)scheduled above, so it hasn't
    // been destroyed yet. Unregister from the poller and free the task.
    for (unsigned b = 0; b < runtime->watcher_bucket_count; b++)
    {
        _bb_runtime_watcher_t *watcher;

        /* Out of the table before the task is destroyed: a task's cleanup
         * callback runs inside bb_task_destroy() and may call back into the
         * runtime, which must find a consistent table. */
        while ((watcher = runtime->watcher_buckets[b]))
        {
            bb_task_t *watched = watcher->task;

            bb_poller_unregister(runtime->poller, watcher->fd, watcher->events);
            _bb_runtime_watcher_remove(runtime, watcher);
            bb_task_destroy(watched);
        }
    }

    while (runtime->free_watchers)
    {
        _bb_runtime_watcher_t *node = runtime->free_watchers;
        runtime->free_watchers = node->chain_next;
        free(node);
    }

    // Clean-up timers
    for (int i = 0; i < bb_timer_heap_count(runtime->timers); i++)
    {
        bb_timer_t *timer = bb_timer_heap_at(runtime->timers, i);

        bb_task_destroy(timer->task);
    }
    bb_timer_heap_destroy(runtime->timers);

    // Clean-up scheduler
    bb_scheduler_destroy(runtime->scheduler);

    bb_poller_destroy(runtime->poller);

    free(runtime->watcher_buckets);

    bb_platform_net_cleanup();

    free(runtime);
}

bb_task_t *bb_runtime_schedule_ex(bb_runtime_t *runtime, const bb_task_config_t *config)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(config);

    if (!task)
    {
        return NULL;
    }

    if (bb_scheduler_schedule(runtime->scheduler, task) != 0)
    {
        bb_task_destroy(task);
        task = NULL;
    }
    return task;
}

static void _bb_runtime_remove_timers(bb_runtime_t *runtime, bb_task_t *task)
{
    bb_timer_heap_remove_task(runtime->timers, task);
}

static void _bb_runtime_remove_watchers(bb_runtime_t *runtime, bb_task_t *task)
{
    _bb_runtime_watcher_t *watcher = task->watcher;

    if (!watcher)
    {
        return;
    }

    bb_poller_unregister(runtime->poller, watcher->fd, watcher->events);
    _bb_runtime_watcher_remove(runtime, watcher);
}

int bb_runtime_cancel_task(bb_runtime_t *runtime, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    if (bb_task_cancel(task) != 0)
    {
        return -1;
    }

    _bb_runtime_remove_watchers(runtime, task);

    _bb_runtime_remove_timers(runtime, task);

    bb_scheduler_schedule(runtime->scheduler, task); // Schedule to be destroyed

    return 0;
}

static void _bb_runtime_wait(bb_runtime_t *runtime, int timeout_ms)
{
    if (!runtime)
    {
        return;
    }

    bb_poll_event_t events[BB_RUNTIME_MAX_EVENTS];

    int ready = bb_poller_wait(runtime->poller, events, BB_RUNTIME_MAX_EVENTS, timeout_ms);

    // bb_poller_wait() returns -1 on a backend-level failure -- e.g. a
    // transient malloc failure while building the ready-list, or (in
    // principle) the underlying epoll/kqueue handle itself misbehaving.
    // That is never fatal to the loop: it just means "no FD-readiness
    // information this tick". Timers and already-scheduled tasks below
    // are completely independent of the poller and still run normally,
    // so we explicitly skip FD dispatch here and let the next
    // bb_runtime_tick() try the poller again. One bad tick must never
    // crash or wedge the runtime.
    if (ready <= 0)
    {
        return;
    }

    for (int i = 0; i < ready; i++)
    {
        _bb_runtime_watcher_t *watcher = _bb_runtime_fd_chain(runtime, events[i].fd);

        while (watcher)
        {
            /* Scheduling only queues the task (nothing runs here), but a
             * one-shot watcher is removed below, so read the successor first. */
            _bb_runtime_watcher_t *next = watcher->chain_next;

            if (watcher->fd == events[i].fd && (watcher->events & events[i].events))
            {
                bb_scheduler_schedule(runtime->scheduler, watcher->task);
                if (watcher->mode == BB_WATCH_ONESHOT)
                {
                    bb_poller_unregister(runtime->poller, watcher->fd, watcher->events);
                    _bb_runtime_watcher_remove(runtime, watcher);
                }
            }

            watcher = next;
        }
    }
}

static void _bb_runtime_update_timers(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    uint64_t now = (uint64_t)bb_time_monotonic_ms();

    bb_timer_t *top;

    // The heap root is always the next timer due to fire, so we only ever
    // need to look at it: pop (or re-arm) it and see whether the new root
    // is due too, instead of scanning every live timer on each tick.
    while ((top = bb_timer_heap_peek(runtime->timers)) && now >= top->next_fire_ms)
    {
        bb_task_t *task = top->task;

        bb_scheduler_schedule(runtime->scheduler, task);

        if (top->repeating)
        {
            uint64_t next_fire_ms = now + top->interval_ms;

            if (next_fire_ms <= now)
            {
                next_fire_ms = now + 1;
            }

            bb_timer_heap_reschedule_top(runtime->timers, next_fire_ms);
        }
        else
        {
            bb_timer_heap_pop(runtime->timers);
        }
    }
}

static int _bb_runtime_next_timeout_ms(bb_runtime_t *runtime)
{
    bb_timer_t *top = bb_timer_heap_peek(runtime->timers);

    if (!top)
    {
        return BB_RUNTIME_IDLE_TIMEOUT_MS;
    }

    uint64_t now = (uint64_t)bb_time_monotonic_ms();

    if (top->next_fire_ms <= now)
    {
        return 0; // already due, don't block at all
    }

    uint64_t delta = top->next_fire_ms - now;
    delta = delta > BB_RUNTIME_IDLE_TIMEOUT_MS ? BB_RUNTIME_IDLE_TIMEOUT_MS : delta;
    return (int)delta;
}

void bb_runtime_tick(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    int wait_timeout = bb_scheduler_is_empty(runtime->scheduler) ? _bb_runtime_next_timeout_ms(runtime) : 0;

    // Schedule FD Events
    _bb_runtime_wait(runtime, wait_timeout);

    // Schedule Timers
    _bb_runtime_update_timers(runtime);

    // Execute scheduled tasks
    bb_task_t *task;

    while (runtime->running && (task = bb_scheduler_next(runtime->scheduler)))
    {
        task->state &= ~BB_TASK_SCHEDULED;

        if (!(task->state & BB_TASK_CANCELLED))
        {
            bb_task_execute(task);
        }
        if (!(task->state & BB_TASK_SCHEDULED || task->state & BB_TASK_PERSISTENT))
        {
            bb_task_destroy(task);
        }
    }
}

void bb_runtime_run(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    runtime->running = true;

    while (runtime->running)
    {
        bb_runtime_tick(runtime);
    }
}

void bb_runtime_set_running(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    runtime->running = true;
}

void bb_runtime_stop(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    runtime->running = false;
}

bool bb_runtime_is_running(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return false;
    }
    return runtime->running;
}

static int _watch_fd(bb_runtime_t *runtime, bb_socket_t fd, int events, bb_watch_mode_t mode, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    _bb_runtime_watcher_t *watcher = _bb_runtime_find_watcher_exact(runtime, fd, events);

    if (watcher)
    {
        bb_task_t *old_task = watcher->task;

        watcher->mode = mode;
        watcher->task = task;
        task->watcher = watcher;
        if (mode == BB_WATCH_PERSISTENT)
        {
            task->state |= BB_TASK_PERSISTENT;
        }

        if (old_task != task)
        {
            old_task->watcher = NULL; // the watcher is the new task's now
            bb_runtime_cancel_task(runtime, old_task); //cancelled AND re-enqueued for destruction
        }
        return 0;
    }

    /* Get the node first: nothing has been changed yet, so failing here needs
     * no rollback. */
    watcher = _bb_runtime_watcher_acquire(runtime);
    if (!watcher)
    {
        return -1;
    }

    int existing_mask = _bb_runtime_fd_registered_mask(runtime, fd);
    int new_mask = existing_mask | events;

    if (new_mask != existing_mask)
    {
        if (existing_mask != 0)
        {
            bb_poller_unregister(runtime->poller, fd, existing_mask);
        }
        if (bb_poller_register(runtime->poller, fd, new_mask) != 0)
        {
            if (existing_mask != 0)
            {
                bb_poller_register(runtime->poller, fd, existing_mask); /* best-effort restore */
            }
            _bb_runtime_watcher_release(runtime, watcher);
            return -1;
        }
    }

    if (mode == BB_WATCH_PERSISTENT)
    {
        task->state |= BB_TASK_PERSISTENT;
    }

    watcher->fd = fd;
    watcher->events = events;
    watcher->mode = mode;
    watcher->task = task;
    task->watcher = watcher;

    _bb_runtime_watcher_link(runtime, watcher);

    return 0;
}

bb_task_t *bb_runtime_watch_fd_ex(bb_runtime_t *runtime, bb_socket_t fd, int events, bb_watch_mode_t mode, const bb_task_config_t *config)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(config);
    if (!task)
    {
        return NULL;
    }
    
    if (_watch_fd(runtime, fd, events, mode, task) != 0)
    {
        bb_task_destroy(task);
        return NULL;
    }

    return task;
}

int bb_runtime_unwatch_fd(bb_runtime_t *runtime, bb_socket_t fd)
{
    if (!runtime)
    {
        return -1;
    }

    int res = -1;
    bb_poller_unregister(runtime->poller, fd, BB_EVENT_READ | BB_EVENT_WRITE);

    for (;;)
    {
        /* Restart from the chain head each time: cancelling a task runs its
         * cleanup, which may itself add or remove watchers. */
        _bb_runtime_watcher_t *watcher = _bb_runtime_fd_chain(runtime, fd);

        while (watcher && watcher->fd != fd)
        {
            watcher = watcher->chain_next;
        }

        if (!watcher)
        {
            break;
        }

        res = 0;
        bb_task_t *task = watcher->task;

        _bb_runtime_watcher_remove(runtime, watcher);
        bb_task_cancel(task);

        bb_scheduler_schedule(runtime->scheduler, task); // Schedule for Destruction
    }

    return res;
}

bb_task_t *bb_runtime_set_interval_ex(bb_runtime_t *runtime, uint64_t interval_ms, const bb_task_config_t *config)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(config);
    if (!task)
    {
        return NULL;
    }

    task->state |= BB_TASK_PERSISTENT;

    uint64_t next_fire_ms = (uint64_t)bb_time_monotonic_ms() + interval_ms;

    if (bb_timer_heap_push(runtime->timers, next_fire_ms, interval_ms, 1, task) != 0)
    {
        bb_task_destroy(task);
        return NULL;
    }

    return task;
}

bb_task_t *bb_runtime_set_timeout_ex(bb_runtime_t *runtime, uint64_t timeout_ms, const bb_task_config_t *config)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(config);
    if (!task)
    {
        return NULL;
    }

    uint64_t next_fire_ms = (uint64_t)bb_time_monotonic_ms() + timeout_ms;

    if (bb_timer_heap_push(runtime->timers, next_fire_ms, timeout_ms, 0, task) != 0)
    {
        bb_task_destroy(task);
        return NULL;
    }

    return task;
}

bool bb_runtime_is_empty(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return true;
    }

    return runtime->watcher_count == 0 && bb_timer_heap_is_empty(runtime->timers) && bb_scheduler_is_empty(runtime->scheduler);
}
