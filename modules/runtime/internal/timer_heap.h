#ifndef BB_RUNTIME_TIMER_HEAP_H
#define BB_RUNTIME_TIMER_HEAP_H

#include <stdbool.h>
#include <stdint.h>

#include "blue-bird/runtime/task.h"

typedef struct {
    uint64_t interval_ms;
    uint64_t next_fire_ms;
    int repeating;
    bb_task_t *task;
} bb_timer_t;

typedef struct bb_timer_heap bb_timer_heap_t;

bb_timer_heap_t *bb_timer_heap_create(void);

void bb_timer_heap_destroy(bb_timer_heap_t *heap);

int bb_timer_heap_push(bb_timer_heap_t *heap, uint64_t next_fire_ms, uint64_t interval_ms, int repeating, bb_task_t *task);

bb_timer_t *bb_timer_heap_peek(bb_timer_heap_t *heap);

void bb_timer_heap_pop(bb_timer_heap_t *heap);

void bb_timer_heap_reschedule_top(bb_timer_heap_t *heap, uint64_t new_next_fire_ms);

int bb_timer_heap_remove_task(bb_timer_heap_t *heap, bb_task_t *task);

bool bb_timer_heap_is_empty(bb_timer_heap_t *heap);

int bb_timer_heap_count(bb_timer_heap_t *heap);

bb_timer_t *bb_timer_heap_at(bb_timer_heap_t *heap, int idx);

#endif
