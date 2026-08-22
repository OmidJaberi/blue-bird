#include <blue-bird/error/assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "scheduler.h"
#include "task_internal.h"

static int order[3];
static int index_pos = 0;

static void task1_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    order[index_pos++] = 1;
}

static void task2_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    order[index_pos++] = 2;
}

static void task3_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    order[index_pos++] = 3;
}

void test_scheduler(void)
{
    bb_scheduler_t *scheduler = bb_scheduler_create();

    BB_ASSERT(scheduler != NULL);

    bb_task_t *t1 = bb_task_create(&(bb_task_config_t) {.run = task1_cb});

    bb_task_t *t2 = bb_task_create(&(bb_task_config_t) {.run = task2_cb});

    bb_task_t *t3 = bb_task_create(&(bb_task_config_t) {.run = task3_cb});

    bb_scheduler_schedule(scheduler, t1);
    bb_scheduler_schedule(scheduler, t2);
    bb_scheduler_schedule(scheduler, t3);

    bb_task_t *task;

    task = bb_scheduler_next(scheduler);
    bb_task_execute(task);
    bb_task_destroy(task);

    task = bb_scheduler_next(scheduler);
    bb_task_execute(task);
    bb_task_destroy(task);

    task = bb_scheduler_next(scheduler);
    bb_task_execute(task);
    bb_task_destroy(task);

    bb_scheduler_destroy(scheduler);

    BB_ASSERT(order[0] == 1);
    BB_ASSERT(order[1] == 2);
    BB_ASSERT(order[2] == 3);
}

static void noop_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
}

static void test_scheduler_empty_scheduler_returns_null(void)
{
    printf("\tRunning test_scheduler_empty_scheduler_returns_null...\n");

    bb_scheduler_t *scheduler = bb_scheduler_create();
    BB_ASSERT(scheduler != NULL);

    BB_ASSERT(bb_scheduler_is_empty(scheduler) == true);
    BB_ASSERT(bb_scheduler_next(scheduler) == NULL);

    bb_scheduler_destroy(scheduler);
}

static void test_scheduler_null_args_are_rejected(void)
{
    printf("\tRunning test_scheduler_null_args_are_rejected...\n");

    bb_scheduler_t *scheduler = bb_scheduler_create();
    BB_ASSERT(scheduler != NULL);

    bb_task_t *t = bb_task_create(&(bb_task_config_t) {.run = noop_cb});
    BB_ASSERT(t != NULL);

    BB_ASSERT(bb_scheduler_schedule(NULL, t) == -1);
    BB_ASSERT(bb_scheduler_schedule(scheduler, NULL) == -1);
    BB_ASSERT(bb_scheduler_next(NULL) == NULL);

    // Must not crash.
    bb_scheduler_destroy(NULL);

    bb_task_destroy(t);
    bb_scheduler_destroy(scheduler);
}

static void test_scheduler_duplicate_schedule_is_idempotent(void)
{
    printf("\tRunning test_scheduler_duplicate_schedule_is_idempotent...\n");

    bb_scheduler_t *scheduler = bb_scheduler_create();
    BB_ASSERT(scheduler != NULL);

    bb_task_t *t1 = bb_task_create(&(bb_task_config_t) {.run = noop_cb});
    bb_task_t *t2 = bb_task_create(&(bb_task_config_t) {.run = noop_cb});
    BB_ASSERT(t1 != NULL && t2 != NULL);

    BB_ASSERT(bb_scheduler_schedule(scheduler, t1) == 0);
    BB_ASSERT(bb_scheduler_schedule(scheduler, t1) == 0); // duplicate, already SCHEDULED
    BB_ASSERT(bb_scheduler_schedule(scheduler, t2) == 0);

    // Only two distinct nodes should exist: t1, then t2.
    BB_ASSERT(bb_scheduler_next(scheduler) == t1);
    BB_ASSERT(bb_scheduler_next(scheduler) == t2);
    BB_ASSERT(bb_scheduler_next(scheduler) == NULL);

    bb_task_destroy(t1);
    bb_task_destroy(t2);
    bb_scheduler_destroy(scheduler);
}

static void test_scheduler_maintains_fifo_across_refills(void)
{
    printf("\tRunning test_scheduler_maintains_fifo_across_refills...\n");

    bb_scheduler_t *scheduler = bb_scheduler_create();
    BB_ASSERT(scheduler != NULL);

    bb_task_t *t1 = bb_task_create(&(bb_task_config_t) {.run = noop_cb});
    bb_task_t *t2 = bb_task_create(&(bb_task_config_t) {.run = noop_cb});
    bb_task_t *t3 = bb_task_create(&(bb_task_config_t) {.run = noop_cb});
    BB_ASSERT(t1 != NULL && t2 != NULL && t3 != NULL);

    // Round 1: schedule + fully drain down to empty.
    BB_ASSERT(bb_scheduler_schedule(scheduler, t1) == 0);
    BB_ASSERT(bb_scheduler_next(scheduler) == t1);
    BB_ASSERT(bb_scheduler_is_empty(scheduler) == true);

    // Round 2: refill after having gone fully empty (tail must have been
    // reset to NULL, or this append silently corrupts the list).
    BB_ASSERT(bb_scheduler_schedule(scheduler, t2) == 0);
    BB_ASSERT(bb_scheduler_schedule(scheduler, t3) == 0);

    BB_ASSERT(bb_scheduler_next(scheduler) == t2);
    BB_ASSERT(bb_scheduler_next(scheduler) == t3);
    BB_ASSERT(bb_scheduler_is_empty(scheduler) == true);

    bb_task_destroy(t1);
    bb_task_destroy(t2);
    bb_task_destroy(t3);
    bb_scheduler_destroy(scheduler);
}

int main(void)
{
    printf("Running Scheduler tests...\n");
    test_scheduler();
    test_scheduler_empty_scheduler_returns_null();
    test_scheduler_null_args_are_rejected();
    test_scheduler_duplicate_schedule_is_idempotent();
    test_scheduler_maintains_fifo_across_refills();
    printf("Scheduler tests passed.\n");
    return 0;
}
