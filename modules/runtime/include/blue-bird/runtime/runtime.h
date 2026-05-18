#ifndef BB_RUNTIME_H
#define BB_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/runtime/task.h"

typedef struct bb_runtime bb_runtime_t;

bb_runtime_t *bb_runtime_create(void);

void bb_runtime_destroy(
    bb_runtime_t *runtime
);

void bb_runtime_run(
    bb_runtime_t *runtime
);

void bb_runtime_stop(
    bb_runtime_t *runtime
);

void bb_runtime_tick(
    bb_runtime_t *runtime
);

int bb_runtime_schedule(
    bb_runtime_t *runtime,
    bb_task_t *task
);


#ifdef __cplusplus
}
#endif

#endif
