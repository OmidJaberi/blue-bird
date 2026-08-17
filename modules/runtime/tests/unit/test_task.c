#include <blue-bird/error/assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "task_internal.h"

static int executed = 0;

static void test_task_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    executed = 1;
}

void test_task(void)
{
    printf("\tRunning test_task...\n");
    bb_task_t *task = bb_task_create(&(bb_task_config_t) {.run = test_task_cb});

    BB_ASSERT(task != NULL);

    bb_task_execute(task);
    bb_task_destroy(task);

    BB_ASSERT(executed == 1);
}

// Cleanup callback execution
static int cleanup_called = 0;
static bb_task_result_t cleanup_result = BB_TASK_RES_ERROR;

static void cleanup_cb(bb_task_t *task, void *userdata, bb_task_result_t result)
{
    (void)task;
    (void)userdata;

    cleanup_called++;
    cleanup_result = result;
}

static void test_task_cleanup(void)
{
    printf("\tRunning test_task_cleanup...\n");

    cleanup_called = 0;
    cleanup_result = BB_TASK_RES_ERROR;

    bb_task_t *task = bb_task_create(&(bb_task_config_t) {
        .run = test_task_cb,
        .cleanup = cleanup_cb
    });

    BB_ASSERT(task != NULL);

    bb_task_execute(task);
    BB_ASSERT(cleanup_called == 0);

    bb_task_destroy(task);

    BB_ASSERT(cleanup_called == 1);
    BB_ASSERT(cleanup_result == BB_TASK_RES_COMPLETED);
}

int main(void)
{
    printf("Running Task tests...\n");
    test_task();
    test_task_cleanup();
    printf("Task tests passed.\n");
    return 0;
}
