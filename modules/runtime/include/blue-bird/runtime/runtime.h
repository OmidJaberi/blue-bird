#ifndef BB_RUNTIME_H
#define BB_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdlib.h>

#include "blue-bird/runtime/task.h"
#include "blue-bird/runtime/event.h"

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

int bb_runtime_watch_fd(
    bb_runtime_t *runtime,
    int fd,
    int events,
    bb_watch_mode_t mode,
    bb_task_t *task
);

int bb_runtime_unwatch_fd(
    bb_runtime_t *runtime,
    int fd
);

int bb_runtime_set_interval(
    bb_runtime_t *runtime,
    uint64_t interval_ms,
    bb_task_t *task
);

int bb_runtime_set_timeout(
    bb_runtime_t *runtime,
    uint64_t timeout_ms,
    bb_task_t *task
);


#ifdef __cplusplus
}
#endif

#endif
