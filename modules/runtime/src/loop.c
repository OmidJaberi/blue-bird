#include <stdlib.h>

#include "blue-bird/runtime/loop.h"
#include "blue-bird/runtime/scheduler.h"
#include "blue-bird/runtime/task.h"

struct bb_loop {
    bb_scheduler_t *scheduler;
    int running;
};

bb_loop_t *bb_loop_create(void)
{
    bb_loop_t *loop = malloc(sizeof(bb_loop_t));

    if (!loop)
    {
        return NULL;
    }

    loop->scheduler = bb_scheduler_create();

    if (!loop->scheduler)
    {
        free(loop);
        return NULL;
    }

    loop->running = 0;

    return loop;
}

void bb_loop_destroy(bb_loop_t *loop)
{
    if (!loop)
    {
        return;
    }

    bb_scheduler_destroy(loop->scheduler);

    free(loop);
}

int bb_loop_schedule(bb_loop_t *loop, bb_task_t *task)
{
    if (!loop || !task)
    {
        return -1;
    }

    return bb_scheduler_schedule(loop->scheduler, task);
}

void bb_loop_stop(bb_loop_t *loop)
{
    if (!loop)
    {
        return;
    }

    loop->running = 0;
}

void bb_loop_run(bb_loop_t *loop)
{
    if (!loop)
    {
        return;
    }

    loop->running = 1;

    while (loop->running)
    {

        bb_task_t *task = bb_scheduler_next(loop->scheduler);

        if (!task)
        {
            break;
        }

        bb_task_execute(task);
    }

    loop->running = 0;
}
