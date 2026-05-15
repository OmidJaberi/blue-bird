#include <stdlib.h>

#include "blue-bird/runtime/task.h"

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

    task->callback(task, task->userdata);
}
