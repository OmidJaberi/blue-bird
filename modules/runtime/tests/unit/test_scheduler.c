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

    bb_task_t *t1 = bb_task_create(task1_cb, NULL);

    bb_task_t *t2 = bb_task_create(task2_cb, NULL);

    bb_task_t *t3 = bb_task_create(task3_cb, NULL);

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

int main(void)
{
    printf("Running Scheduler tests...\n");
    test_scheduler();
    printf("Scheduler tests passed.\n");
    return 0;
}
