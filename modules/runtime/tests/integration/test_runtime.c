#include <blue-bird/error/assert.h>
#include <stdio.h>

#include "blue-bird/runtime/runtime.h"
#include "runtime_internal.h"

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
    runtime->running = true;
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

    cancellation_runtime->running = true;
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

    runtime->running = true;
    while (!bb_runtime_is_empty(runtime))
    {
        bb_runtime_tick(runtime);
    }

    BB_ASSERT(chain_depth == MAX_CHAIN_DEPTH);

    bb_runtime_destroy(runtime);
}

// Runtime Stop

static int stop_test_first_executed = 0;
static int stop_test_second_executed = 0;

static void stop_test_first_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    stop_test_first_executed++;

    bb_runtime_stop(runtime);
}

static void stop_test_second_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    stop_test_second_executed++;
}

static void test_runtime_stop(void)
{
    printf("\tRunning test_runtime_stop...\n");

    stop_test_first_executed = 0;
    stop_test_second_executed = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_runtime_schedule(runtime, stop_test_first_cb, runtime) != NULL);

    BB_ASSERT(bb_runtime_schedule(runtime, stop_test_second_cb, runtime) != NULL);

    bb_runtime_run(runtime);

    BB_ASSERT(stop_test_first_executed == 1);
    BB_ASSERT(stop_test_second_executed == 0);

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

// Cancel from other task:
static int cross_cancel_target_executed = 0;
static bb_task_t *cross_cancel_target = NULL;

static void cross_cancel_target_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    cross_cancel_target_executed++;
}

static void cross_cancel_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    bb_runtime_cancel_task(runtime, cross_cancel_target);
}

static void test_cross_task_cancellation(void)
{
    printf("\tRunning test_cross_task_cancellation...\n");

    cross_cancel_target_executed = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_runtime_schedule(runtime, cross_cancel_cb, runtime) != NULL);

    cross_cancel_target = bb_runtime_schedule(runtime, cross_cancel_target_cb, NULL);
    BB_ASSERT(cross_cancel_target != NULL);

    bb_runtime_set_running(runtime);
    while (!bb_runtime_is_empty(runtime))
    {
        bb_runtime_tick(runtime);
    }

    BB_ASSERT(cross_cancel_target_executed == 0);

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

    runtime->running = true;
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
    runtime->running = true;
    while (!bb_runtime_is_empty(runtime))
        bb_runtime_tick(runtime);

    BB_ASSERT(cancelled_timeout_executed == 0);
    BB_ASSERT(bb_runtime_is_empty(runtime));

    bb_runtime_destroy(runtime);
}

// Interval canecellation before first execution
static int cancelled_interval_executed = 0;

static void cancelled_interval_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    cancelled_interval_executed++;
}

static void test_interval_cancellation(void)
{
    printf("\tRunning test_interval_cancellation...\n");

    cancelled_interval_executed = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    bb_task_t *interval =
        bb_runtime_set_interval(
            runtime,
            10,
            cancelled_interval_cb,
            NULL
        );

    BB_ASSERT(interval != NULL);

    bb_runtime_cancel_task(runtime, interval);

    BB_ASSERT(bb_task_is_cancelled(interval) == 1);

    runtime->running = true;
    while (!bb_runtime_is_empty(runtime))
        bb_runtime_tick(runtime);

    BB_ASSERT(cancelled_interval_executed == 0);

    bb_runtime_destroy(runtime);
}

// Interval canecellation cleanup
static int cleanup_interval_counter = 0;
static int cleanup_interval_called = 0;
static bb_runtime_t *cleanup_interval_runtime;

static void cleanup_interval_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    cleanup_interval_counter++;
    if (cleanup_interval_counter == 3)
    {
        bb_runtime_cancel_task(cleanup_interval_runtime, task);
    }
}

static void cleanup_interval_cleanup(bb_task_t *task, void *userdata, bb_task_result_t result)
{
    (void)task;
    (void)userdata;
    (void)result;

    cleanup_interval_called = 1;
}

static void test_interval_cleanup(void)
{
    printf("\tRunning test_interval_cleanup...\n");

    cleanup_interval_counter = 0;
    cleanup_interval_called = 0;

    cleanup_interval_runtime = bb_runtime_create();
    BB_ASSERT(cleanup_interval_runtime != NULL);

    bb_task_t *interval = bb_runtime_set_interval_ex(cleanup_interval_runtime, 10, &(bb_task_config_t) {
        .run = cleanup_interval_cb,
        .cleanup = cleanup_interval_cleanup,
        .userdata = NULL,
    });

    BB_ASSERT(interval != NULL);

    cleanup_interval_runtime->running = true;
    while (!bb_runtime_is_empty(cleanup_interval_runtime))
        bb_runtime_tick(cleanup_interval_runtime);

    BB_ASSERT(cleanup_interval_counter == 3);
    BB_ASSERT(cleanup_interval_called == 1);

    bb_runtime_destroy(cleanup_interval_runtime);
}

// Zero delay timeout

static int zero_timeout_executed = 0;

static void zero_timeout_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    zero_timeout_executed++;

    bb_runtime_stop(runtime);
}

static void test_zero_timeout(void)
{
    printf("\tRunning test_zero_timeout...\n");

    zero_timeout_executed = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(
        bb_runtime_set_timeout(
            runtime,
            0,
            zero_timeout_cb,
            runtime
        ) != NULL
    );

    bb_runtime_run(runtime);

    BB_ASSERT(zero_timeout_executed == 1);

    bb_runtime_destroy(runtime);
}

// Empty runtime Destruction

static void test_empty_runtime_destroy(void)
{
    printf("\tRunning test_empty_runtime_destroy...\n");

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_runtime_is_empty(runtime));

    bb_runtime_destroy(runtime);
}

// Self cancellation:
static int self_cancel_executed = 0;
static int self_cancel_after = 0;

static void self_cancel_cb(bb_task_t *task, void *userdata)
{
    bb_runtime_t *runtime = userdata;

    self_cancel_executed++;

    bb_runtime_cancel_task(runtime, task);

    self_cancel_after++;
    bb_runtime_stop(runtime);
}

static void test_self_cancellation(void)
{
    printf("\tRunning test_self_cancellation...\n");

    self_cancel_executed = 0;
    self_cancel_after = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    bb_task_t *task = bb_runtime_schedule(runtime, self_cancel_cb, runtime);

    BB_ASSERT(task != NULL);

    bb_runtime_run(runtime);

    BB_ASSERT(self_cancel_executed == 1);
    BB_ASSERT(self_cancel_after == 1);
    BB_ASSERT(bb_task_is_cancelled(task) == 1);

    bb_runtime_destroy(runtime);
}

// Schedule then cancel
static int scheduled_then_cancelled_executed = 0;

static void scheduled_then_cancelled_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    scheduled_then_cancelled_executed++;
}

static void schedule_then_cancel_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    bb_task_t *new_task = bb_runtime_schedule(runtime, scheduled_then_cancelled_cb, NULL);

    BB_ASSERT(new_task != NULL);

    bb_runtime_cancel_task(runtime, new_task);
    BB_ASSERT(bb_task_is_cancelled(new_task) == 1);
}

static void test_schedule_then_cancel(void)
{
    printf("\tRunning test_schedule_then_cancel...\n");

    scheduled_then_cancelled_executed = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_runtime_schedule(runtime, schedule_then_cancel_cb, runtime) != NULL);

    runtime->running = true;

    while (!bb_runtime_is_empty(runtime))
        bb_runtime_tick(runtime);

    BB_ASSERT(scheduled_then_cancelled_executed == 0);

    bb_runtime_destroy(runtime);
}

// Selective Cancellation
static int selective_cancel_counter = 0;
static bb_task_t *selective_cancel_target = NULL;

static void selective_cancel_target_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    selective_cancel_counter += 1000;
}

static void selective_cancel_normal_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    selective_cancel_counter++;
}

static void selective_cancel_controller_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    bb_runtime_cancel_task(runtime, selective_cancel_target);
}

static void test_selective_cancellation(void)
{
    printf("\tRunning test_selective_cancellation...\n");

    selective_cancel_counter = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    /*
     * Controller executes first and cancels the target.
     */
    BB_ASSERT(bb_runtime_schedule(runtime, selective_cancel_controller_cb, runtime) != NULL);

    selective_cancel_target = bb_runtime_schedule(runtime, selective_cancel_target_cb, NULL);

    BB_ASSERT(selective_cancel_target != NULL);

    /*
     * These should survive cancellation of the target.
     */
    BB_ASSERT(bb_runtime_schedule(runtime, selective_cancel_normal_cb, NULL) != NULL);

    BB_ASSERT(bb_runtime_schedule(runtime, selective_cancel_normal_cb, NULL) != NULL);

    runtime->running = true;

    while (!bb_runtime_is_empty(runtime))
        bb_runtime_tick(runtime);

    BB_ASSERT(selective_cancel_counter == 2);

    bb_runtime_destroy(runtime);
}

// Mid-Interval Cancellation
static int interval_spawn_counter = 0;
static int interval_spawn_ticks = 0;

static void interval_spawn_task_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    interval_spawn_counter++;
}

static void interval_spawn_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    interval_spawn_ticks++;

    BB_ASSERT(bb_runtime_schedule(runtime, interval_spawn_task_cb, NULL) != NULL);

    if (interval_spawn_ticks == 3)
    {
        bb_runtime_stop(runtime);
    }
}

static void test_interval_task_interaction(void)
{
    printf("\tRunning test_interval_task_interaction...\n");

    interval_spawn_counter = 0;
    interval_spawn_ticks = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    bb_task_t *interval = bb_runtime_set_interval(runtime, 10, interval_spawn_cb, runtime);

    BB_ASSERT(interval != NULL);

    bb_runtime_run(runtime);

    BB_ASSERT(interval_spawn_counter == 2);
    BB_ASSERT(interval_spawn_ticks == 3);

    bb_runtime_destroy(runtime);
}

// Multi Fanout roots
#define MULTI_FANOUT_ROOTS 100
#define MULTI_FANOUT_PER_ROOT 100

static int multi_fanout_counter = 0;

static void multi_fanout_leaf_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    multi_fanout_counter++;
}

static void multi_fanout_root_cb(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_runtime_t *runtime = userdata;

    for (int i = 0; i < MULTI_FANOUT_PER_ROOT; i++)
    {
        BB_ASSERT(bb_runtime_schedule(runtime, multi_fanout_leaf_cb, NULL) != NULL);
    }
}

static void test_multi_task_fanout(void)
{
    printf("\tRunning test_multi_task_fanout...\n");

    multi_fanout_counter = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    for (int i = 0; i < MULTI_FANOUT_ROOTS; i++)
    {
        BB_ASSERT(bb_runtime_schedule(runtime, multi_fanout_root_cb, runtime) != NULL);
    }

    runtime->running = true;

    while (!bb_runtime_is_empty(runtime))
        bb_runtime_tick(runtime);

    BB_ASSERT(multi_fanout_counter == MULTI_FANOUT_ROOTS * MULTI_FANOUT_PER_ROOT);

    bb_runtime_destroy(runtime);
}

// BB_WATCH_ONESHOT: watcher should fire once, then stop being invoked
// even though the fd stays readable.
static int oneshot_fire_count = 0;

static void oneshot_watch_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    oneshot_fire_count++;
}

static void test_oneshot_watch_fires_once(void)
{
    printf("\tRunning test_oneshot_watch_fires_once...\n");

    oneshot_fire_count = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    bb_socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(listener != BB_INVALID_SOCKET);

    int opt = 1;
    BB_ASSERT(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) == 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    BB_ASSERT(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    BB_ASSERT(listen(listener, 1) == 0);

    socklen_t addr_len = sizeof(addr);
    BB_ASSERT(getsockname(listener, (struct sockaddr *)&addr, &addr_len) == 0);

    bb_socket_t client = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(client != BB_INVALID_SOCKET);

    BB_ASSERT(connect(client, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    bb_socket_t server = accept(listener, NULL, NULL);
    BB_ASSERT(server != BB_INVALID_SOCKET);

    bb_socket_close(listener);

    // Make the server socket readable and leave the data unread.
    BB_ASSERT(send(client, "x", 1, 0) == 1);

    bb_task_t *watch = bb_runtime_watch_fd(runtime, server, BB_EVENT_READ, BB_WATCH_ONESHOT, oneshot_watch_cb, NULL);
    BB_ASSERT(watch != NULL);

    // First tick: fd is readable, watcher should fire exactly once.
    runtime->running = true;
    bb_runtime_tick(runtime);   // poll -> schedules the watch task
    bb_runtime_tick(runtime);   // executes the watch task

    BB_ASSERT(oneshot_fire_count == 1);

    // Data is still sitting in the pipe (never read), so the fd is still
    // readable. A correctly-implemented oneshot watch must NOT fire again.
    bb_runtime_tick(runtime);
    bb_runtime_tick(runtime);

    BB_ASSERT(oneshot_fire_count == 1);

    bb_socket_close(server);
    bb_socket_close(client);

    bb_runtime_destroy(runtime);

    bb_platform_net_cleanup();
}

// Re-watching the same (fd, events) pair with a different task must
// REPLACE the previous watcher: the old task is cancelled (never fires),
// only the new task's watcher is fired and remains registered.
static int watch_replace_old_fired = 0;
static int watch_replace_new_fired = 0;

static void watch_replace_old_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
    watch_replace_old_fired++;
}

static void watch_replace_new_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;
    watch_replace_new_fired++;
}

static void test_watch_fd_replace_cancels_old_task(void)
{
    printf("\tRunning test_watch_fd_replace_cancels_old_task...\n");

    watch_replace_old_fired = 0;
    watch_replace_new_fired = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    BB_ASSERT(bb_platform_net_init() == 0);

    bb_socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(listener != BB_INVALID_SOCKET);

    int opt = 1;
    BB_ASSERT(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) == 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    BB_ASSERT(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    BB_ASSERT(listen(listener, 1) == 0);

    socklen_t addr_len = sizeof(addr);
    BB_ASSERT(getsockname(listener, (struct sockaddr *)&addr, &addr_len) == 0);

    bb_socket_t client = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(client != BB_INVALID_SOCKET);
    BB_ASSERT(connect(client, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    bb_socket_t server = accept(listener, NULL, NULL);
    BB_ASSERT(server != BB_INVALID_SOCKET);

    bb_socket_close(listener);

    // Make the server socket readable so a watcher would have something
    // to fire on.
    BB_ASSERT(send(client, "x", 1, 0) == 1);

    bb_task_t *old_task = bb_runtime_watch_fd(runtime, server, BB_EVENT_READ, BB_WATCH_ONESHOT, watch_replace_old_cb, NULL);
    BB_ASSERT(old_task != NULL);
    BB_ASSERT(runtime->watcher_count == 1);

    // Re-watch the exact same (fd, events) with a different task before
    // the runtime ever ticks. This must replace, not duplicate, the
    // watcher, and must cancel `old_task` immediately.
    bb_task_t *new_task = bb_runtime_watch_fd(runtime, server, BB_EVENT_READ, BB_WATCH_ONESHOT, watch_replace_new_cb, NULL);
    BB_ASSERT(new_task != NULL);
    BB_ASSERT(new_task != old_task);

    BB_ASSERT(bb_task_is_cancelled(old_task) == 1);
    BB_ASSERT(runtime->watcher_count == 1); // still one watcher, not two

    runtime->running = true;
    bb_runtime_tick(runtime);   // poll -> schedules the (new) watch task, and destroys the cancelled old task
    bb_runtime_tick(runtime);   // executes the scheduled tasks

    BB_ASSERT(watch_replace_old_fired == 0);
    BB_ASSERT(watch_replace_new_fired == 1);

    bb_socket_close(server);
    bb_socket_close(client);

    bb_runtime_destroy(runtime);

    bb_platform_net_cleanup();
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
    test_runtime_stop();
    test_runtime_reuse();
    test_timer_to_task_scheduling();
    test_timer_order();
    test_cross_task_cancellation();
    test_task_fanout();
    test_timeout_cancellation();
    test_interval_cancellation();
    test_interval_cleanup();
    test_zero_timeout();
    test_empty_runtime_destroy();
    test_self_cancellation();
    test_schedule_then_cancel();
    test_selective_cancellation();
    test_interval_task_interaction();
    test_multi_task_fanout();
    test_oneshot_watch_fires_once();
    test_watch_fd_replace_cancels_old_task();
    printf("Runtime integration test passed.\n");
    return 0;
}
