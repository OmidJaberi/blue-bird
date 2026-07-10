#ifndef BB_RUNTIME_SCHEDULER_H
#define BB_RUNTIME_SCHEDULER_H

#include "blue-bird/runtime/task.h"

typedef struct bb_scheduler bb_scheduler_t;

bb_scheduler_t *bb_scheduler_create(void);

void bb_scheduler_destroy(bb_scheduler_t *scheduler);

int bb_scheduler_schedule(bb_scheduler_t *scheduler, bb_task_t *task);

bb_task_t *bb_scheduler_next(bb_scheduler_t *scheduler);

#endif
