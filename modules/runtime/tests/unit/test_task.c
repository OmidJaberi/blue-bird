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


// Cancellation finalizes immediately
static int cancel_cleanup_called = 0;
static bb_task_result_t cancel_cleanup_result = BB_TASK_RES_ERROR;

static void cancel_cleanup_cb(bb_task_t *task, void *userdata, bb_task_result_t result)
{
    (void)task;
    (void)userdata;

    cancel_cleanup_called++;
    cancel_cleanup_result = result;
}

static void test_task_cancel_cleanup(void)
{
    printf("\tRunning test_task_cancel_cleanup...\n");

    cancel_cleanup_called = 0;
    cancel_cleanup_result = BB_TASK_RES_ERROR;

    bb_task_t *task = bb_task_create(&(bb_task_config_t) {
        .run = test_task_cb,
        .cleanup = cancel_cleanup_cb
    });

    BB_ASSERT(task != NULL);

    BB_ASSERT(bb_task_cancel(task) == 0);

    // Cancellation finalizes immediately.
    BB_ASSERT(cancel_cleanup_called == 1);
    BB_ASSERT(cancel_cleanup_result == BB_TASK_RES_CANCELLED);
    BB_ASSERT(bb_task_is_cancelled(task) == 1);

    // Destroy must not invoke cleanup a second time.
    bb_task_destroy(task);

    BB_ASSERT(cancel_cleanup_called == 1);
}


// Cleanup must only run once
static int once_cleanup_called = 0;

static void once_cleanup_cb(bb_task_t *task, void *userdata, bb_task_result_t result)
{
    (void)task;
    (void)userdata;
    (void)result;

    once_cleanup_called++;
}

static void test_task_cleanup_once(void)
{
    printf("\tRunning test_task_cleanup_once...\n");

    once_cleanup_called = 0;

    bb_task_t *task = bb_task_create(&(bb_task_config_t) {
        .run = test_task_cb,
        .cleanup = once_cleanup_cb
    });

    BB_ASSERT(task != NULL);

    bb_task_execute(task);

    bb_task_cancel(task);
    bb_task_destroy(task);
    BB_ASSERT(once_cleanup_called == 1);
}


// Cancellation before execution must prevent execution
static int cancel_before_execute_called = 0;

static void cancel_before_execute_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    cancel_before_execute_called++;
}

static void test_task_cancel_before_execute(void)
{
    printf("\tRunning test_task_cancel_before_execute...\n");

    cancel_before_execute_called = 0;

    bb_task_t *task = bb_task_create(&(bb_task_config_t) {
        .run = cancel_before_execute_cb
    });

    BB_ASSERT(task != NULL);

    BB_ASSERT(bb_task_cancel(task) == 0);
    BB_ASSERT(bb_task_is_cancelled(task) == 1);

    bb_task_execute(task);

    BB_ASSERT(cancel_before_execute_called == 0);

    bb_task_destroy(task);
}


// Scheduling state transitions
static void test_task_state_transitions(void)
{
    printf("\tRunning test_task_state_transitions...\n");

    bb_task_t *task = bb_task_create(&(bb_task_config_t) {
        .run = test_task_cb
    });

    BB_ASSERT(task != NULL);

    // Newly created tasks are idle.
    BB_ASSERT(bb_task_is_scheduled(task) == 0);
    BB_ASSERT(bb_task_is_cancelled(task) == 0);

    // Simulate scheduler ownership.
    task->state |= BB_TASK_SCHEDULED;

    BB_ASSERT(bb_task_is_scheduled(task) == 1);

    bb_task_execute(task);

    // Execution clears the scheduled state.
    BB_ASSERT(bb_task_is_scheduled(task) == 0);
    BB_ASSERT((task->state & BB_TASK_RUNNING) == 0);

    bb_task_destroy(task);
}

int main(void)
{
    printf("Running Task tests...\n");
    test_task();
    test_task_cleanup();
    test_task_cancel_cleanup();
    test_task_cleanup_once();
    test_task_cancel_before_execute();
    test_task_state_transitions();
    printf("Task tests passed.\n");
    return 0;
}
