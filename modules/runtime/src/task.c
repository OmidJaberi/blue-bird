#include <stdlib.h>

#include "task_internal.h"

bb_task_t *bb_task_create(const bb_task_config_t *config)
{
    if (!config || !config->run)
    {
        return NULL;
    }

    bb_task_t *task = calloc(1, sizeof(*task));

    if (!task)
    {
        return NULL;
    }

    task->config = *config;
    task->state = BB_TASK_IDLE;

    return task;
}

void bb_task_destroy(bb_task_t *task)
{
    if (!task)
    {
        return;
    }

    if (task->config.cleanup)
    {
        task->config.cleanup(task, task->config.userdata, BB_TASK_RES_COMPLETED); // Temporary: BB_TASK_RES_COMPLETED
    }

    free(task);
}

void bb_task_execute(bb_task_t *task)
{
    if (!task || !task->config.run)
    {
        return;
    }

    task->state &= ~BB_TASK_SCHEDULED;
    task->state |= BB_TASK_RUNNING;

    task->config.run(task, task->config.userdata);

    task->state &= ~BB_TASK_RUNNING;
}

int bb_task_cancel(bb_task_t *task)
{
    if (!task)
    {
        return -1;
    }

    task->state |= BB_TASK_CANCELLED;
    task->state &= ~BB_TASK_PERSISTENT;

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
