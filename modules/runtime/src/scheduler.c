#include <stdlib.h>

#include "scheduler.h"
#include "task_internal.h"

typedef struct _bb_task_node {
    bb_task_t *task;
    struct _bb_task_node *next;
} _bb_task_node_t;

typedef struct bb_scheduler {
    _bb_task_node_t *head;
    _bb_task_node_t *tail;
} bb_scheduler_t;

bb_scheduler_t *bb_scheduler_create(void)
{
    bb_scheduler_t *scheduler = malloc(sizeof(bb_scheduler_t));

    if (!scheduler)
    {
        return NULL;
    }

    scheduler->head = NULL;
    scheduler->tail = NULL;

    return scheduler;
}

void bb_scheduler_destroy(bb_scheduler_t *scheduler)
{
    if (!scheduler)
    {
        return;
    }

    _bb_task_node_t *current = scheduler->head;

    while (current)
    {

        _bb_task_node_t *next = current->next;

        free(current);

        current = next;
    }

    free(scheduler);
}

int bb_scheduler_schedule(bb_scheduler_t *scheduler, bb_task_t *task)
{
    if (!scheduler || !task)
    {
        return -1;
    }

    if (task->state & BB_TASK_SCHEDULED)
    {
        return 0;
    }

    _bb_task_node_t *node = malloc(sizeof(*node));

    if (!node)
    {
        return -1;
    }

    node->task = task;
    node->next = NULL;

    task->state |= BB_TASK_SCHEDULED;

    if (!scheduler->head)
    {
        scheduler->head = node;
        scheduler->tail = node;

        return 0;
    }

    scheduler->tail->next = node;
    scheduler->tail = node;

    return 0;
}

bb_task_t *bb_scheduler_next(bb_scheduler_t *scheduler)
{
    if (!scheduler || !scheduler->head)
    {
        return NULL;
    }

    _bb_task_node_t *node = scheduler->head;

    scheduler->head = node->next;

    if (!scheduler->head)
    {
        scheduler->tail = NULL;
    }

    bb_task_t *task = node->task;

    free(node);

    return task;
}

bool bb_scheduler_is_empty(bb_scheduler_t *scheduler)
{
    return (scheduler->head == NULL);
}
