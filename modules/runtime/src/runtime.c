#include <stdlib.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/scheduler.h"
#include "blue-bird/runtime/poller.h"

#define BB_RUNTIME_MAX_WATCHERS 1024

typedef struct {
    int fd;
    int events;
    bb_task_t *task;
} _bb_runtime_watcher_t;

struct bb_runtime {
    int running;
    bb_scheduler_t *scheduler;
    bb_poller_t *poller;

    _bb_runtime_watcher_t watchers[BB_RUNTIME_MAX_WATCHERS];
    int watcher_count;
};

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

    for (int i = 0; i < ready; i++)
    {
        for (int j = 0; j < runtime->watcher_count; j++)
        {
            _bb_runtime_watcher_t *watcher = &runtime->watchers[j];

            if (watcher->fd == events[i].fd)
            {
                bb_scheduler_schedule(runtime->scheduler, watcher->task);
            }
        }
    }

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

int bb_runtime_watch_fd(bb_runtime_t *runtime, int fd, int events, bb_task_t *task)
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
