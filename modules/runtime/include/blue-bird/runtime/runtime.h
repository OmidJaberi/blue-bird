#ifndef BB_RUNTIME_H
#define BB_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "blue-bird/utils/platform.h"

#include "blue-bird/runtime/task.h"
#include "blue-bird/runtime/event.h"

typedef struct bb_runtime bb_runtime_t;

bb_runtime_t *bb_runtime_default(void);

bb_runtime_t *bb_runtime_create(void);

void bb_runtime_destroy(bb_runtime_t *runtime);

void bb_runtime_run(bb_runtime_t *runtime);

void bb_runtime_stop(bb_runtime_t *runtime);

bool bb_runtime_is_running(bb_runtime_t *runtime);

void bb_runtime_tick(bb_runtime_t *runtime);

bb_task_t *bb_runtime_schedule_ex(bb_runtime_t *runtime, const bb_task_config_t *config);

int bb_runtime_cancel_task(bb_runtime_t *runtime, bb_task_t *task);

bb_task_t *bb_runtime_watch_fd_ex(bb_runtime_t *runtime, bb_socket_t fd, int events, bb_watch_mode_t mode, const bb_task_config_t *config);

int bb_runtime_unwatch_fd(bb_runtime_t *runtime, bb_socket_t fd);

bb_task_t *bb_runtime_set_interval_ex(bb_runtime_t *runtime, uint64_t interval_ms, const bb_task_config_t *config);

bb_task_t *bb_runtime_set_timeout_ex(bb_runtime_t *runtime, uint64_t timeout_ms, const bb_task_config_t *config);

bool bb_runtime_is_empty(bb_runtime_t *runtime);


static inline bb_task_t *bb_runtime_schedule(bb_runtime_t *runtime, bb_task_cb callback, void *userdata)
{
    return bb_runtime_schedule_ex(runtime, &(bb_task_config_t) {
        .run = callback,
        .userdata = userdata
    });
}

static inline bb_task_t *bb_runtime_watch_fd(bb_runtime_t *runtime, bb_socket_t fd, int events, bb_watch_mode_t mode, bb_task_cb callback, void *userdata)
{
    return bb_runtime_watch_fd_ex(runtime, fd, events, mode, &(bb_task_config_t) {
        .run = callback,
        .userdata = userdata
    });
}

static inline bb_task_t *bb_runtime_set_interval(bb_runtime_t *runtime, uint64_t interval_ms, bb_task_cb callback, void *userdata)
{
    return bb_runtime_set_interval_ex(runtime, interval_ms, &(bb_task_config_t) {
        .run = callback,
        .userdata = userdata
    });
}

static inline bb_task_t *bb_runtime_set_timeout(bb_runtime_t *runtime, uint64_t timeout_ms, bb_task_cb callback, void *userdata)
{
    return bb_runtime_set_timeout_ex(runtime, timeout_ms, &(bb_task_config_t) {
        .run = callback,
        .userdata = userdata
    });
}

static inline void bb_runtime_run_default(void)
{
    bb_runtime_run(bb_runtime_default());
}


#ifdef __cplusplus
}
#endif

#endif
