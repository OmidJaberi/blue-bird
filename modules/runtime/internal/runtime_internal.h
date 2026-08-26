#ifndef BB_RUNTIME_INTERNAL_H
#define BB_RUNTIME_INTERNAL_H


#include "blue-bird/runtime/runtime.h"
#include "blue-bird/utils/platform.h"

#include "scheduler.h"
#include "poller.h"
#include "task_internal.h"
#include "timer_heap.h"


#define BB_RUNTIME_WATCHERS_INITIAL_CAPACITY 16
#define BB_RUNTIME_MAX_EVENTS 64 // Max Event Batch
#define BB_RUNTIME_IDLE_TIMEOUT_MS 1000  // max time to block with nothing scheduled

typedef struct {
    bb_socket_t fd;
    int events;
    bb_watch_mode_t mode;
    bb_task_t *task;
} _bb_runtime_watcher_t;

struct bb_runtime {
    bool running;
    bb_scheduler_t *scheduler;
    bb_poller_t *poller;

    _bb_runtime_watcher_t *watchers; // heap-allocated, grown as needed
    int watcher_count;
    int watcher_capacity;

    bb_timer_heap_t *timers; // min-heap keyed by next_fire_ms, grown as needed
};


#endif
