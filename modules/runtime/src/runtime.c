#include <stdlib.h>
#include <time.h>

#include "blue-bird/runtime/runtime.h"
#include "task_internal.h"
#include "scheduler.h"
#include "poller.h"

#include <signal.h>

static void _init_signals(void)
{
#if defined(SIGPIPE) && !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif
}

#define BB_RUNTIME_MAX_WATCHERS 1024
#define BB_RUNTIME_MAX_TIMERS 1024

static bb_runtime_t *g_runtime = NULL;

typedef struct {
    int fd;
    int events;
    bb_watch_mode_t mode;
    bb_task_t *task;
} _bb_runtime_watcher_t;

typedef struct {
    uint64_t interval_ms;
    uint64_t next_fire_ms;
    int repeating;
    bb_task_t *task;
} _bb_runtime_timer_t;

struct bb_runtime {
    int running;
    bb_scheduler_t *scheduler;
    bb_poller_t *poller;

    _bb_runtime_watcher_t watchers[BB_RUNTIME_MAX_WATCHERS];
    int watcher_count;

    _bb_runtime_timer_t timers[BB_RUNTIME_MAX_TIMERS];
    int timer_count;
};

static uint64_t _bb_runtime_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

bb_runtime_t *bb_runtime_default(void)
{
    if (!g_runtime)
    {
        g_runtime = bb_runtime_create();
    }
    return g_runtime;
}

bb_runtime_t *bb_runtime_create(void)
{
    _init_signals();

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

    runtime->running = 0;

    return runtime;
}

void bb_runtime_destroy(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    bb_scheduler_destroy(runtime->scheduler);

    bb_poller_destroy(runtime->poller);

    free(runtime);
}

bb_task_t *bb_runtime_schedule(bb_runtime_t *runtime, bb_task_cb callback, void *userdata)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(callback, userdata);

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
    for (int i = 0; i < runtime->timer_count;)
    {
        if (runtime->timers[i].task == task)
        {
            runtime->timers[i] = runtime->timers[runtime->timer_count - 1];

            runtime->timer_count--;
        }
        else
        {
            i++;
        }
    }
}

static void _bb_runtime_remove_watchers(bb_runtime_t *runtime, bb_task_t *task)
{
    for (int i = 0; i < runtime->watcher_count;)
    {
        if (runtime->watchers[i].task == task)
        {
            bb_poller_unregister(runtime->poller, runtime->watchers[i].fd, runtime->watchers[i].events);
            runtime->watchers[i] = runtime->watchers[runtime->watcher_count - 1];
            runtime->watcher_count--;
        }
        else
        {
            i++;
        }
    }
}

int bb_runtime_cancel_task(bb_runtime_t *runtime, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    bb_task_cancel(task);

    _bb_runtime_remove_watchers(runtime, task);

    _bb_runtime_remove_timers(runtime, task);

    return 0;
}

void bb_runtime_tick(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    bb_poll_event_t events[64];

    int ready = bb_poller_wait(runtime->poller, events, 64, 10);

    /*
     * FD Events
     */
    for (int i = 0; i < ready; i++)
    {
        for (int j = 0; j < runtime->watcher_count; j++)
        {
            _bb_runtime_watcher_t *watcher = &runtime->watchers[j];

            if (watcher->fd == events[i].fd && (watcher->events & events[i].events))
            {
                bb_scheduler_schedule(runtime->scheduler, watcher->task);

                /*
                 * One-shot watchers
                 * auto-remove after fire
                 */
                if (watcher->mode == BB_WATCH_ONESHOT)
                {
                    bb_runtime_cancel_task(runtime, watcher->task);

                    /*
                     * watcher array compacted,
                     * so revisit current index
                     */
                    j--;
                }
            }
        }
    }

    /*
     * Timers
     */
    uint64_t now = _bb_runtime_now_ms();

    for (int i = 0; i < runtime->timer_count;)
    {
        _bb_runtime_timer_t *timer = &runtime->timers[i];

        if (now >= timer->next_fire_ms)
        {
            bb_scheduler_schedule(runtime->scheduler, timer->task);

            if (timer->repeating)
            {
                timer->next_fire_ms = now + timer->interval_ms;
                i++;
            }
            else
            {
                runtime->timers[i] = runtime->timers[runtime->timer_count - 1];
                runtime->timer_count--;
            }
        }
        else
        {
            i++;
        }
    }

    /*
     * Execute scheduled tasks
     */
    bb_task_t *task;

    while ((task = bb_scheduler_next(runtime->scheduler)))
    {
        if (!(task->state & BB_TASK_CANCELLED))
        {
            bb_task_execute(task);
        }
        if (task->state & BB_TASK_PERSISTENT)
        {
            continue;
        }
        if (!(task->state & BB_TASK_SCHEDULED))
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

    runtime->running = 1;

    while (runtime->running)
    {
        bb_runtime_tick(runtime);
    }
}

void bb_runtime_stop(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    runtime->running = 0;
}

static int _bb_runtime_find_watcher_exact(bb_runtime_t *runtime, int fd, int events)
{
    for (int i = 0; i < runtime->watcher_count; i++)
    {
        if (runtime->watchers[i].fd == fd && runtime->watchers[i].events == events)
        {
            return i;
        }
    }
    return -1;
}

static int _bb_runtime_fd_registered_mask(bb_runtime_t *runtime, int fd)
{
    int mask = 0;

    for (int i = 0; i < runtime->watcher_count; i++)
    {
        if (runtime->watchers[i].fd == fd)
        {
            mask |= runtime->watchers[i].events;
        }
    }
    return mask;
}

static int _watch_fd(bb_runtime_t *runtime, int fd, int events, bb_watch_mode_t mode, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    /*
     * Exact same (fd, events) watcher already exists -> this is a re-arm,
     * swap the task, don't touch other event types on this fd.
     */
    int idx = _bb_runtime_find_watcher_exact(runtime, fd, events);

    if (idx >= 0)
    {
        _bb_runtime_watcher_t *watcher = &runtime->watchers[idx];
        bb_task_t *old_task = watcher->task;

        watcher->mode = mode;
        watcher->task = task;
        task->state |= BB_TASK_PERSISTENT;

        if (old_task != task)
        {
            bb_task_cancel(old_task); /* see destroy caveat below */
        }
        return 0;
    }

    /*
     * New watcher for this fd. It may be a brand-new fd, or a new event
     * type layered onto an fd we already watch (e.g. adding WRITE to
     * an fd we already watch for READ) -- either way, don't unregister
     * events that other watchers on this fd still care about.
     */
    if (runtime->watcher_count >= BB_RUNTIME_MAX_WATCHERS)
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
            return -1;
        }
    }

    task->state |= BB_TASK_PERSISTENT;

    _bb_runtime_watcher_t *watcher = &runtime->watchers[runtime->watcher_count];

    watcher->fd = fd;
    watcher->events = events;
    watcher->mode = mode;
    watcher->task = task;

    runtime->watcher_count++;

    return 0;
}

bb_task_t *bb_runtime_watch_fd(bb_runtime_t *runtime, int fd, int events, bb_watch_mode_t mode, bb_task_cb callback, void *userdata)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(callback, userdata);

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

int bb_runtime_unwatch_fd(bb_runtime_t *runtime, int fd)
{
    if (!runtime)
    {
        return -1;
    }

    bb_poller_unregister(runtime->poller, fd, BB_EVENT_READ | BB_EVENT_WRITE);

    for (int i = 0; i < runtime->watcher_count;)
    {
        if (runtime->watchers[i].fd == fd)
        {
            bb_task_cancel(runtime->watchers[i].task);
            runtime->watchers[i] = runtime->watchers[runtime->watcher_count - 1];
            runtime->watcher_count--;
        }
        else
        {
            i++;
        }
    }

    return 0;
}

bb_task_t *bb_runtime_set_interval(bb_runtime_t *runtime, uint64_t interval_ms, bb_task_cb callback, void *userdata)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(callback, userdata);

    if (!task)
    {
        return NULL;
    }

    if (runtime->timer_count >= BB_RUNTIME_MAX_TIMERS)
    {
        bb_task_destroy(task);
        return NULL;
    }

    task->state |= BB_TASK_PERSISTENT;

    _bb_runtime_timer_t *timer = &runtime->timers[runtime->timer_count];

    timer->interval_ms = interval_ms;
    timer->next_fire_ms = _bb_runtime_now_ms() + interval_ms;
    timer->repeating = 1;
    timer->task = task;
    runtime->timer_count++;

    return task;
}

bb_task_t *bb_runtime_set_timeout(bb_runtime_t *runtime, uint64_t timeout_ms, bb_task_cb callback, void *userdata)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(callback, userdata);

    if (!task)
    {
        return NULL;
    }

    if (runtime->timer_count >= BB_RUNTIME_MAX_TIMERS)
    {
        bb_task_destroy(task);
        return NULL;
    }

    _bb_runtime_timer_t *timer = &runtime->timers[runtime->timer_count];

    timer->interval_ms = timeout_ms;
    timer->next_fire_ms = _bb_runtime_now_ms() + timeout_ms;
    timer->repeating = 0;
    timer->task = task;
    runtime->timer_count++;

    return 0;
}

bool bb_runtime_is_empty(bb_runtime_t *runtime)
{
    if (runtime->watcher_count > 0 || runtime->timer_count > 0 || !bb_scheduler_is_empty(runtime->scheduler))
    {
        return false;
    }
    return true;
}
