#ifndef BB_RUNTIME_INTERNAL_H
#define BB_RUNTIME_INTERNAL_H


#include "blue-bird/runtime/runtime.h"
#include "blue-bird/utils/platform.h"

#include "scheduler.h"
#include "poller.h"
#include "task_internal.h"


#define BB_RUNTIME_MAX_WATCHERS 1024
#define BB_RUNTIME_MAX_TIMERS 1024
#define BB_RUNTIME_MAX_EVENTS 64 // Max Event Batch
#define BB_RUNTIME_IDLE_TIMEOUT_MS 1000  // max time to block with nothing scheduled

typedef struct {
    bb_socket_t fd;
    int events;
    bb_watch_mode_t mode;
    bb_task_t *task;
} _bb_runtime_watcher_t;

typedef struct {
    uint64_t interval_ms;
    uint64_t next_fire_ms;
    int repeating;
    bb_task_t *task;
} _bb_runtime_timer_t;

struct bb_runtime {
    bool running;
    bb_scheduler_t *scheduler;
    bb_poller_t *poller;

    _bb_runtime_watcher_t watchers[BB_RUNTIME_MAX_WATCHERS];
    int watcher_count;

    _bb_runtime_timer_t timers[BB_RUNTIME_MAX_TIMERS];
    int timer_count;
};


#endif
