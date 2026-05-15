#ifndef BB_RUNTIME_LOOP_H
#define BB_RUNTIME_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/runtime/task.h"

typedef struct bb_loop bb_loop_t;

bb_loop_t *bb_loop_create(void);

void bb_loop_destroy(
    bb_loop_t *loop
);

void bb_loop_run(
    bb_loop_t *loop
);

void bb_loop_stop(
    bb_loop_t *loop
);

int bb_loop_schedule(
    bb_loop_t *loop,
    bb_task_t *task
);


#ifdef __cplusplus
}
#endif

#endif
