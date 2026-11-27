#ifndef BB_RUNTIME_INTERNAL_H
#define BB_RUNTIME_INTERNAL_H


#include "blue-bird/runtime/runtime.h"
#include "blue-bird/utils/platform.h"

#include "scheduler.h"
#include "poller.h"
#include "task_internal.h"
#include "timer_heap.h"


#define BB_RUNTIME_WATCHER_BUCKETS_INITIAL 64 // power of two
#define BB_RUNTIME_MAX_EVENTS 64 // Max Event Batch
#define BB_RUNTIME_IDLE_TIMEOUT_MS 1000  // max time to block with nothing scheduled

/*
 * One watch on one fd for one event mask, owned by the task that services it.
 *
 * Watchers live in a hash table keyed by fd, so finding the ones on a
 * descriptor (to dispatch an event, to work out what the poller must be
 * registered for, to replace a watch) costs the length of one short chain
 * rather than a scan of every watcher. Nodes are separately allocated and never
 * move, which is what lets a task point straight at its own watcher and a chain
 * be unlinked in O(1).
 */
typedef struct _bb_runtime_watcher {
    bb_socket_t fd;
    int events;
    bb_watch_mode_t mode;
    bb_task_t *task;

    struct _bb_runtime_watcher *chain_next;
    struct _bb_runtime_watcher **chain_prev; // the link that points at this node
} _bb_runtime_watcher_t;

struct bb_runtime {
    bool running;
    bb_scheduler_t *scheduler;
    bb_poller_t *poller;

    _bb_runtime_watcher_t **watcher_buckets; // fd hash table; chains keep registration order
    unsigned watcher_bucket_count;           // power of two
    int watcher_count;
    _bb_runtime_watcher_t *free_watchers;    // released nodes, reused to avoid malloc churn

    bb_timer_heap_t *timers; // min-heap keyed by next_fire_ms, grown as needed
};


/* The watcher registered on exactly (fd, events), or NULL. */
_bb_runtime_watcher_t *_bb_runtime_find_watcher_exact(bb_runtime_t *runtime, bb_socket_t fd, int events);

#endif
