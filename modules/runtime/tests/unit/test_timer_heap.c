#include <blue-bird/error/assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "timer_heap.h"
#include "task_internal.h"

static void noop_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

static bb_task_t *make_task(void)
{
    return bb_task_create(&(bb_task_config_t) {.run = noop_cb});
}

static void test_timer_heap_peek_returns_soonest(void)
{
    printf("\tRunning test_timer_heap_peek_returns_soonest...\n");

    bb_timer_heap_t *heap = bb_timer_heap_create();
    BB_ASSERT(heap != NULL);

    bb_task_t *t1 = make_task();
    bb_task_t *t2 = make_task();
    bb_task_t *t3 = make_task();

    bb_timer_heap_push(heap, 300, 0, 0, t1);
    bb_timer_heap_push(heap, 100, 0, 0, t2);
    bb_timer_heap_push(heap, 200, 0, 0, t3);

    bb_timer_t *top = bb_timer_heap_peek(heap);
    BB_ASSERT(top != NULL);
    BB_ASSERT(top->task == t2);
    BB_ASSERT(top->next_fire_ms == 100);

    bb_timer_heap_pop(heap);
    top = bb_timer_heap_peek(heap);
    BB_ASSERT(top->task == t3);

    bb_timer_heap_pop(heap);
    top = bb_timer_heap_peek(heap);
    BB_ASSERT(top->task == t1);

    bb_timer_heap_pop(heap);
    BB_ASSERT(bb_timer_heap_peek(heap) == NULL);
    BB_ASSERT(bb_timer_heap_is_empty(heap));

    bb_task_destroy(t1);
    bb_task_destroy(t2);
    bb_task_destroy(t3);
    bb_timer_heap_destroy(heap);
}

static void test_timer_heap_no_hardcoded_cap(void)
{
    printf("\tRunning test_timer_heap_no_hardcoded_cap...\n");

    // The old array-backed implementation capped out at 1024 live timers.
    // Push well past that to prove the heap grows instead of failing.
    const int total = 5000;

    bb_timer_heap_t *heap = bb_timer_heap_create();
    BB_ASSERT(heap != NULL);

    bb_task_t **tasks = malloc(sizeof(bb_task_t *) * (size_t)total);
    BB_ASSERT(tasks != NULL);

    for (int i = 0; i < total; i++)
    {
        tasks[i] = make_task();
        // Push in descending fire time so the min always changes, exercising sift-up.
        int rc = bb_timer_heap_push(heap, (uint64_t)(total - i), 0, 0, tasks[i]);
        BB_ASSERT(rc == 0);
    }

    BB_ASSERT(bb_timer_heap_count(heap) == total);

    uint64_t last = 0;
    int popped = 0;

    while (!bb_timer_heap_is_empty(heap))
    {
        bb_timer_t *top = bb_timer_heap_peek(heap);
        BB_ASSERT(top->next_fire_ms >= last); // must come out in non-decreasing order
        last = top->next_fire_ms;
        bb_timer_heap_pop(heap);
        popped++;
    }

    BB_ASSERT(popped == total);

    for (int i = 0; i < total; i++)
    {
        bb_task_destroy(tasks[i]);
    }
    free(tasks);
    bb_timer_heap_destroy(heap);
}

static void test_timer_heap_remove_task(void)
{
    printf("\tRunning test_timer_heap_remove_task...\n");

    bb_timer_heap_t *heap = bb_timer_heap_create();
    BB_ASSERT(heap != NULL);

    bb_task_t *t1 = make_task();
    bb_task_t *t2 = make_task();
    bb_task_t *t3 = make_task();

    bb_timer_heap_push(heap, 10, 0, 0, t1);
    bb_timer_heap_push(heap, 20, 0, 0, t2);
    bb_timer_heap_push(heap, 30, 0, 0, t3);

    int removed = bb_timer_heap_remove_task(heap, t2);
    BB_ASSERT(removed == 1);
    BB_ASSERT(bb_timer_heap_count(heap) == 2);

    bb_timer_t *top = bb_timer_heap_peek(heap);
    BB_ASSERT(top->task == t1);
    bb_timer_heap_pop(heap);

    top = bb_timer_heap_peek(heap);
    BB_ASSERT(top->task == t3);
    bb_timer_heap_pop(heap);

    BB_ASSERT(bb_timer_heap_is_empty(heap));

    bb_task_destroy(t1);
    bb_task_destroy(t2);
    bb_task_destroy(t3);
    bb_timer_heap_destroy(heap);
}

static void test_timer_heap_reschedule_top(void)
{
    printf("\tRunning test_timer_heap_reschedule_top...\n");

    bb_timer_heap_t *heap = bb_timer_heap_create();
    BB_ASSERT(heap != NULL);

    bb_task_t *t1 = make_task();
    bb_task_t *t2 = make_task();

    bb_timer_heap_push(heap, 10, 5, 1, t1);
    bb_timer_heap_push(heap, 50, 0, 0, t2);

    // t1 fires at 10 and re-arms 5ms out, landing before t2 (still due at 50).
    bb_timer_heap_reschedule_top(heap, 15);

    bb_timer_t *top = bb_timer_heap_peek(heap);
    BB_ASSERT(top->task == t1);
    BB_ASSERT(top->next_fire_ms == 15);

    // Re-arm again, this time past t2's fire time -- t2 should surface as the new min.
    bb_timer_heap_reschedule_top(heap, 100);

    top = bb_timer_heap_peek(heap);
    BB_ASSERT(top->task == t2);

    bb_task_destroy(t1);
    bb_task_destroy(t2);
    bb_timer_heap_destroy(heap);
}

int main(void)
{
    printf("Running Timer Heap tests...\n");

    test_timer_heap_peek_returns_soonest();
    test_timer_heap_no_hardcoded_cap();
    test_timer_heap_remove_task();
    test_timer_heap_reschedule_top();

    printf("Timer Heap tests passed.\n");
    return 0;
}
