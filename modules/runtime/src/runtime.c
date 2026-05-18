#include <stdlib.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/scheduler.h"
#include "blue-bird/runtime/poller.h"

struct bb_runtime {
    int running;
    bb_scheduler_t *scheduler;
    bb_poller_t *poller;
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

    /*
     * Future:
     * poll fd events
     */
    bb_poller_poll(runtime->poller);

    /*
     * Execute ready tasks
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
