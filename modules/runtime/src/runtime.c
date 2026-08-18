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
        bb_task_destroy(task);
    }

    // Clean-up watchers
    // Anything still here was never (re)scheduled above, so it hasn't
    // been destroyed yet. Unregister from the poller and free the task.
    for (int i = 0; i < runtime->watcher_count; i++)
    {
        _bb_runtime_watcher_t *watcher = &runtime->watchers[i];

        bb_poller_unregister(runtime->poller, watcher->fd, watcher->events);
        bb_task_destroy(watcher->task);
    }
    runtime->watcher_count = 0;

    // Clean-up timers
    for (int i = 0; i < runtime->timer_count; i++)
    {
        _bb_runtime_timer_t *timer = &runtime->timers[i];

        bb_task_destroy(timer->task);
    }
    runtime->timer_count = 0;

    // Clean-up scheduler
    bb_scheduler_destroy(runtime->scheduler);

    bb_poller_destroy(runtime->poller);

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
    for (int i = 0; i < runtime->timer_count; i++)
    {
        if (runtime->timers[i].task == task)
        {
            runtime->timers[i] = runtime->timers[runtime->timer_count - 1];

            runtime->timer_count--;
            i--;
        }
    }
}

static void _bb_runtime_remove_watchers(bb_runtime_t *runtime, bb_task_t *task)
{
    for (int i = 0; i < runtime->watcher_count; i++)
    {
        if (runtime->watchers[i].task == task)
        {
            bb_poller_unregister(runtime->poller, runtime->watchers[i].fd, runtime->watchers[i].events);
            runtime->watchers[i] = runtime->watchers[runtime->watcher_count - 1];
            runtime->watcher_count--;
            i--;
        }
    }
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

    for (int i = 0; i < ready; i++)
    {
        for (int j = 0; j < runtime->watcher_count; j++)
        {
            _bb_runtime_watcher_t *watcher = &runtime->watchers[j];

            if (watcher->fd == events[i].fd && (watcher->events & events[i].events))
            {
                bb_scheduler_schedule(runtime->scheduler, watcher->task);
            }
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

    for (int i = 0; i < runtime->timer_count; i++)
    {
        _bb_runtime_timer_t *timer = &runtime->timers[i];

        if (now >= timer->next_fire_ms)
        {
            bb_scheduler_schedule(runtime->scheduler, timer->task);

            if (timer->repeating)
            {
                timer->next_fire_ms = now + timer->interval_ms;
            }
            else
            {
                runtime->timers[i] = runtime->timers[runtime->timer_count - 1];
                runtime->timer_count--;
                i--;
            }
        }
    }
}

static int _bb_runtime_next_timeout_ms(bb_runtime_t *runtime)
{
    if (runtime->timer_count == 0)
    {
        return BB_RUNTIME_IDLE_TIMEOUT_MS;
    }

    uint64_t now = (uint64_t)bb_time_monotonic_ms();
    uint64_t earliest = runtime->timers[0].next_fire_ms;

    for (int i = 1; i < runtime->timer_count; i++)
    {
        if (runtime->timers[i].next_fire_ms < earliest)
        {
            earliest = runtime->timers[i].next_fire_ms;
        }
    }

    if (earliest <= now)
    {
        return 0; // already due, don't block at all
    }

    uint64_t delta = earliest - now;
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

static int _bb_runtime_find_watcher_exact(bb_runtime_t *runtime, bb_socket_t fd, int events)
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

static int _bb_runtime_fd_registered_mask(bb_runtime_t *runtime, bb_socket_t fd)
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

static int _watch_fd(bb_runtime_t *runtime, bb_socket_t fd, int events, bb_watch_mode_t mode, bb_task_t *task)
{
    if (!runtime || !task)
    {
        return -1;
    }

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
            bb_runtime_cancel_task(runtime, old_task); //cancelled AND re-enqueued for destruction
        }
        return 0;
    }

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

    bb_poller_unregister(runtime->poller, fd, BB_EVENT_READ | BB_EVENT_WRITE);

    for (int i = 0; i < runtime->watcher_count; i++)
    {
        if (runtime->watchers[i].fd == fd)
        {
            bb_task_cancel(runtime->watchers[i].task);
            runtime->watchers[i] = runtime->watchers[runtime->watcher_count - 1];
            runtime->watcher_count--;
            i--;
        }
    }

    return 0;
}

bb_task_t *bb_runtime_set_interval_ex(bb_runtime_t *runtime, uint64_t interval_ms, const bb_task_config_t *config)
{
    if (!runtime || runtime->timer_count >= BB_RUNTIME_MAX_TIMERS)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(config);
    if (!task)
    {
        return NULL;
    }

    task->state |= BB_TASK_PERSISTENT;

    _bb_runtime_timer_t *timer = &runtime->timers[runtime->timer_count];

    timer->interval_ms = interval_ms;
    timer->next_fire_ms = (uint64_t)bb_time_monotonic_ms() + interval_ms;
    timer->repeating = 1;
    timer->task = task;
    runtime->timer_count++;

    return task;
}

bb_task_t *bb_runtime_set_timeout_ex(bb_runtime_t *runtime, uint64_t timeout_ms, const bb_task_config_t *config)
{
    if (!runtime || runtime->timer_count >= BB_RUNTIME_MAX_TIMERS)
    {
        return NULL;
    }

    bb_task_t *task = bb_task_create(config);
    if (!task)
    {
        return NULL;
    }

    _bb_runtime_timer_t *timer = &runtime->timers[runtime->timer_count];

    timer->interval_ms = timeout_ms;
    timer->next_fire_ms = (uint64_t)bb_time_monotonic_ms() + timeout_ms;
    timer->repeating = 0;
    timer->task = task;
    runtime->timer_count++;

    return task;
}

bool bb_runtime_is_empty(bb_runtime_t *runtime)
{
    return runtime->watcher_count == 0 && runtime->timer_count == 0 && bb_scheduler_is_empty(runtime->scheduler);
}
