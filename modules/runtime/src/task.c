#include <stdlib.h>

#include "task_internal.h"

bb_task_t *bb_task_create(bb_task_cb callback, void *userdata)
{
    if (!callback)
    {
        return NULL;
    }

    bb_task_t *task = malloc(sizeof(bb_task_t));

    if (!task)
    {
        return NULL;
    }

    task->callback = callback;
    task->userdata = userdata;
    task->state = BB_TASK_IDLE;

    return task;
}

void bb_task_destroy(bb_task_t *task)
{
    if (!task)
    {
        return;
    }

    free(task);
}

void bb_task_execute(bb_task_t *task)
{
    if (!task || !task->callback)
    {
        return;
    }

    task->state &= ~BB_TASK_SCHEDULED;
    task->state |= BB_TASK_RUNNING;

    task->callback(task, task->userdata);

    task->state &= ~BB_TASK_RUNNING;
}

int bb_task_cancel(bb_task_t *task)
{
    if (!task)
    {
        return -1;
    }

    task->state |= BB_TASK_CANCELLED;

    return 0;
}

int bb_task_is_cancelled(const bb_task_t *task)
{
    return task && (task->state & BB_TASK_CANCELLED);
}

int bb_task_is_scheduled(const bb_task_t *task)
{
    return task && (task->state & BB_TASK_SCHEDULED);
}
