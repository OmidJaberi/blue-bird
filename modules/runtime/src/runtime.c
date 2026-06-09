#include <stdlib.h>
#include <time.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/scheduler.h"
#include "blue-bird/runtime/poller.h"

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
    bb_runtime_t *runtime = malloc(sizeof(bb_runtime_t));

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

int bb_runtime_schedule(bb_runtime_t *runtime, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    return bb_scheduler_schedule(runtime->scheduler, task);
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

            if (watcher->fd == events[i].fd)
            {
                bb_scheduler_schedule(runtime->scheduler, watcher->task);

                /*
                 * One-shot watchers
                 * auto-remove after fire
                 */
                if (watcher->mode == BB_WATCH_ONESHOT)
                {
                    bb_runtime_unwatch_fd(runtime, watcher->fd);

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
        bb_task_execute(task);
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

int bb_runtime_watch_fd(bb_runtime_t *runtime, int fd, int events, bb_watch_mode_t mode, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    if (runtime->watcher_count >= BB_RUNTIME_MAX_WATCHERS)
    {
        return -1;
    }

    if (bb_poller_register(runtime->poller, fd, events) != 0)
    {
        return -1;
    }

    _bb_runtime_watcher_t *watcher = &runtime->watchers[runtime->watcher_count];

    watcher->fd = fd;
    watcher->events = events;
    watcher->mode = mode;
    watcher->task = task;

    runtime->watcher_count++;

    return 0;
}

int bb_runtime_unwatch_fd(bb_runtime_t *runtime, int fd)
{
    if (!runtime)
    {
        return -1;
    }

    bb_poller_unregister(runtime->poller, fd);

    for (int i = 0; i < runtime->watcher_count; i++)
    {

        if (runtime->watchers[i].fd == fd)
        {
            runtime->watchers[i] = runtime->watchers[runtime->watcher_count - 1];
            runtime->watcher_count--;
            return 0;
        }
    }

    return -1;
}

int bb_runtime_set_interval(bb_runtime_t *runtime, uint64_t interval_ms, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    if (runtime->timer_count >= BB_RUNTIME_MAX_TIMERS)
    {
        return -1;
    }

    _bb_runtime_timer_t *timer = &runtime->timers[runtime->timer_count];

    timer->interval_ms = interval_ms;
    timer->next_fire_ms = _bb_runtime_now_ms() + interval_ms;
    timer->repeating = 1;
    timer->task = task;
    runtime->timer_count++;

    return 0;
}

int bb_runtime_set_timeout(bb_runtime_t *runtime, uint64_t timeout_ms, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

    if (runtime->timer_count >= BB_RUNTIME_MAX_TIMERS)
    {
        return -1;
    }

    _bb_runtime_timer_t *timer = &runtime->timers[runtime->timer_count];

    timer->interval_ms = timeout_ms;
    timer->next_fire_ms = _bb_runtime_now_ms() + timeout_ms;
    timer->repeating = 0;
    timer->task = task;
    runtime->timer_count++;

    return 0;
}
