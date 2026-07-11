#ifndef BB_RUNTIME_SCHEDULER_H
#define BB_RUNTIME_SCHEDULER_H

#include "blue-bird/runtime/task.h"

typedef struct _bb_task_node {
    bb_task_t *task;
    struct _bb_task_node *next;
} _bb_task_node_t;

typedef struct bb_scheduler {
    _bb_task_node_t *head;
    _bb_task_node_t *tail;
} bb_scheduler_t;

bb_scheduler_t *bb_scheduler_create(void);

void bb_scheduler_destroy(bb_scheduler_t *scheduler);

int bb_scheduler_schedule(bb_scheduler_t *scheduler, bb_task_t *task);

bb_task_t *bb_scheduler_next(bb_scheduler_t *scheduler);

#endif
