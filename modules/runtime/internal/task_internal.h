#ifndef BB_RUNTIME_TASK_INTERNAL_H
#define BB_RUNTIME_TASK_INTERNAL_H

#include "blue-bird/runtime/task.h"

typedef enum {
    BB_TASK_IDLE       = 0,
    BB_TASK_SCHEDULED  = 1 << 0,
    BB_TASK_RUNNING    = 1 << 1,
    BB_TASK_CANCELLED  = 1 << 2,
    BB_TASK_PERSISTENT = 1 << 3
} bb_task_state_t;

struct bb_task {
    bb_task_cb callback;
    void *userdata;

    unsigned state;
};

bb_task_t *bb_task_create(bb_task_cb callback, void *userdata);

void bb_task_destroy(bb_task_t *task);

int bb_task_cancel(bb_task_t *task);

void bb_task_execute(bb_task_t *task);

#endif
