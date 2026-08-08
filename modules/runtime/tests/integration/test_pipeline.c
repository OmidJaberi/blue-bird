#include <blue-bird/error/assert.h>
#include <stdio.h>

#include "blue-bird/runtime/runtime.h"

static int execution_order[10];
static int execution_index = 0;

typedef struct {
    bb_runtime_t *runtime;
} runtime_test_ctx_t;

static void task_c_cb(bb_task_t *task, void *userdata)
{
    (void) task;
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 3;

    printf("\t\tTask C executed\n");

    bb_runtime_stop(ctx->runtime);
}

static void task_b_cb(bb_task_t *task, void *userdata)
{
    (void) task;
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 2;

    printf("\t\tTask B executed\n");

    BB_ASSERT(bb_runtime_schedule(ctx->runtime, task_c_cb, ctx) != NULL);
}

static void task_a_cb(bb_task_t *task, void *userdata)
{
    (void) task;
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 1;

    printf("\t\tTask A executed\n");

    BB_ASSERT(bb_runtime_schedule(ctx->runtime, task_b_cb, ctx) != NULL);
}

void test_runtime_chain(void)
{
    printf("\tTest Runtime chain...\n");
    bb_runtime_t *runtime = bb_runtime_create();

    BB_ASSERT(runtime != NULL);

    runtime_test_ctx_t ctx = {
        .runtime = runtime
    };

    BB_ASSERT(bb_runtime_schedule(runtime, task_a_cb, &ctx) != NULL);

    bb_runtime_run(runtime);

    // Validate execution chain

    BB_ASSERT(execution_index == 3);

    BB_ASSERT(execution_order[0] == 1);
    BB_ASSERT(execution_order[1] == 2);
    BB_ASSERT(execution_order[2] == 3);

    bb_runtime_destroy(runtime);
}

// State for the stress test
static int stress_counter = 0;

static void stress_task_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
    stress_counter++;
}

static void test_massive_task_scheduling(void)
{
    printf("\tRunning test_massive_task_scheduling...\n");
    stress_counter = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    // Schedule 10,000 tasks
    const int NUM_TASKS = 10000;
    for (int i = 0; i < NUM_TASKS; i++)
    {
        bb_runtime_schedule(runtime, stress_task_cb, NULL);
    }

    // Since these aren't chaining, they should all execute in one run pass.
    // We tick until empty.
    while (!bb_runtime_is_empty(runtime))
    {
        bb_runtime_tick(runtime);
    }

    BB_ASSERT(stress_counter == NUM_TASKS);
    
    bb_runtime_destroy(runtime);
}

// State for cancellation test
bb_runtime_t *cancellation_runtime;
static int cancel_target_executed = 0;

static void cancel_target_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
    cancel_target_executed++;
}

static void test_task_cancellation(void)
{
    printf("\tRunning test_task_cancellation...\n");
    cancel_target_executed = 0;

    cancellation_runtime = bb_runtime_create();
    BB_ASSERT(cancellation_runtime != NULL);

    // Schedule a task, but keep its reference to cancel it
    bb_task_t *target_task = bb_runtime_schedule(cancellation_runtime, cancel_target_cb, NULL);

    // Cancel the target task before it gets a chance to run
    bb_runtime_cancel_task(cancellation_runtime, target_task);
    BB_ASSERT(bb_task_is_cancelled(target_task) == 1);

    while (!bb_runtime_is_empty(cancellation_runtime))
    {
        bb_runtime_tick(cancellation_runtime);
    }

    // Target should be cancelled
    BB_ASSERT(cancel_target_executed == 0);

    bb_runtime_destroy(cancellation_runtime);
}

// State for timeout test
static int timeout_executed = 0;

static void timeout_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    bb_runtime_t *runtime = (bb_runtime_t *)userdata;
    timeout_executed++;
    bb_runtime_stop(runtime);
}

static void test_timeout_scheduling(void)
{
    printf("\tRunning test_timeout_scheduling...\n");
    timeout_executed = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    // Schedule a task to run after 50ms
    bb_runtime_set_timeout(runtime, 50, timeout_cb, runtime);

    // Run the loop. It should block/wait until the timeout expires.
    bb_runtime_run(runtime);

    BB_ASSERT(timeout_executed == 1);

    bb_runtime_destroy(runtime);
}

// Interval Timer test
static int interval_counter = 0;
static bb_task_t *interval_task_ref = NULL;

static void interval_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    bb_runtime_t *runtime = (bb_runtime_t *)userdata;
    interval_counter++;
    
    // Stop the interval after 5 executions
    if (interval_counter >= 5)
    {
        bb_runtime_cancel_task(runtime, interval_task_ref);
        bb_runtime_stop(runtime);
    }
}

static void test_interval_scheduling(void)
{
    printf("\tRunning test_interval_scheduling...\n");
    interval_counter = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    // Schedule a task to run every 10ms
    interval_task_ref = bb_runtime_set_interval(runtime, 10, interval_cb, runtime);

    // Run the loop until the callback stops it
    bb_runtime_run(runtime);

    BB_ASSERT(interval_counter == 5);

    bb_runtime_destroy(runtime);
}

// Deep Task Chaining (Recursive Scheduling) test
static int chain_depth = 0;
#define MAX_CHAIN_DEPTH 5000

static void chain_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    bb_runtime_t *runtime = (bb_runtime_t *)userdata;
    chain_depth++;
    
    // Reschedule itself until the max depth is reached
    if (chain_depth < MAX_CHAIN_DEPTH)
    {
        bb_runtime_schedule(runtime, chain_cb, runtime);
    }
}

static void test_deep_task_chaining(void)
{
    printf("\tRunning test_deep_task_chaining...\n");
    chain_depth = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    // Kick off the chain
    bb_runtime_schedule(runtime, chain_cb, runtime);

    while (!bb_runtime_is_empty(runtime))
    {
        bb_runtime_tick(runtime);
    }

    BB_ASSERT(chain_depth == MAX_CHAIN_DEPTH);

    bb_runtime_destroy(runtime);
}

// Runtime Reuse
static int reuse_counter = 0;

static void reuse_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    reuse_counter++;
    bb_runtime_stop(runtime);
}

static void test_runtime_reuse(void)
{
    printf("\tRunning test_runtime_reuse...\n");

    reuse_counter = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_runtime_schedule(runtime, reuse_cb, runtime) != NULL);

    bb_runtime_run(runtime);

    BB_ASSERT(reuse_counter == 1);
    BB_ASSERT(bb_runtime_is_empty(runtime));

    // Schedule another task after the first run completed.
    BB_ASSERT(bb_runtime_schedule(runtime, reuse_cb, runtime) != NULL);

    bb_runtime_run(runtime);

    BB_ASSERT(reuse_counter == 2);
    BB_ASSERT(bb_runtime_is_empty(runtime));

    bb_runtime_destroy(runtime);
}

// Scheduling in Timer
static int timer_to_task_stage = 0;

static void timer_to_task_task_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    BB_ASSERT(timer_to_task_stage == 1);

    timer_to_task_stage = 2;

    bb_runtime_stop(runtime);
}

static void timer_to_task_timer_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    BB_ASSERT(timer_to_task_stage == 0);

    timer_to_task_stage = 1;

    BB_ASSERT(bb_runtime_schedule(runtime, timer_to_task_task_cb, runtime) != NULL);
}

static void test_timer_to_task_scheduling(void)
{
    printf("\tRunning test_timer_to_task_scheduling...\n");

    timer_to_task_stage = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_runtime_set_timeout(runtime, 10, timer_to_task_timer_cb, runtime ) != NULL);

    bb_runtime_run(runtime);

    BB_ASSERT(timer_to_task_stage == 2);

    bb_runtime_destroy(runtime);
}

// Timer order
static int timer_order[3];
static int timer_order_index = 0;

static void timer_order_cb_1(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    timer_order[timer_order_index++] = 1;
}

static void timer_order_cb_2(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    timer_order[timer_order_index++] = 2;
}

static void timer_order_cb_3(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    timer_order[timer_order_index++] = 3;

    bb_runtime_stop(runtime);
}

static void test_timer_order(void)
{
    printf("\tRunning test_timer_order...\n");

    timer_order_index = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    // Schedule deliberately different deadlines.
    BB_ASSERT(bb_runtime_set_timeout(runtime, 30, timer_order_cb_3, runtime) != NULL);

    BB_ASSERT(bb_runtime_set_timeout(runtime, 10, timer_order_cb_1, runtime) != NULL);

    BB_ASSERT(bb_runtime_set_timeout(runtime, 20, timer_order_cb_2, runtime) != NULL);

    bb_runtime_run(runtime);

    BB_ASSERT(timer_order_index == 3);

    BB_ASSERT(timer_order[0] == 1);
    BB_ASSERT(timer_order[1] == 2);
    BB_ASSERT(timer_order[2] == 3);

    bb_runtime_destroy(runtime);
}

// Task Fanout

#define FANOUT_TASKS 1000

static int fanout_counter = 0;

static void fanout_leaf_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    fanout_counter++;
}

static void fanout_root_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    for (int i = 0; i < FANOUT_TASKS; i++)
    {
        BB_ASSERT(bb_runtime_schedule(runtime, fanout_leaf_cb, NULL) != NULL);
    }
}

static void test_task_fanout(void)
{
    printf("\tRunning test_task_fanout...\n");

    fanout_counter = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_runtime_schedule(runtime, fanout_root_cb, runtime) != NULL);

    while (!bb_runtime_is_empty(runtime))
        bb_runtime_tick(runtime);

    BB_ASSERT(fanout_counter == FANOUT_TASKS);

    bb_runtime_destroy(runtime);
}

// Timer cancellation
static int cancelled_timeout_executed = 0;

static void cancelled_timeout_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    cancelled_timeout_executed++;
}

static void test_timeout_cancellation(void)
{
    printf("\tRunning test_timeout_cancellation...\n");

    cancelled_timeout_executed = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    bb_task_t *timeout_task = bb_runtime_set_timeout(runtime, 50, cancelled_timeout_cb, NULL);

    BB_ASSERT(timeout_task != NULL);

    bb_runtime_cancel_task(runtime, timeout_task);

    BB_ASSERT(bb_task_is_cancelled(timeout_task) == 1);

    /*
     * The runtime should process the cancelled timer and eventually
     * become empty without invoking its callback.
     */
    while (!bb_runtime_is_empty(runtime))
        bb_runtime_tick(runtime);

    BB_ASSERT(cancelled_timeout_executed == 0);
    BB_ASSERT(bb_runtime_is_empty(runtime));

    bb_runtime_destroy(runtime);
}

int main(void)
{
    printf("Starting runtime integration test...\n");
    test_runtime_chain();
    test_massive_task_scheduling();
    test_task_cancellation();
    test_timeout_scheduling();
    test_interval_scheduling();
    test_deep_task_chaining();
    test_runtime_reuse();
    test_timer_to_task_scheduling();
    test_timer_order();
    test_task_fanout();
    test_timeout_cancellation();
    printf("Runtime integration test passed.\n");
    return 0;
}
