#include <assert.h>
#include <stdio.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/task.h"

static int execution_order[10];
static int execution_index = 0;

typedef struct {
    bb_runtime_t *runtime;
} runtime_test_ctx_t;

static void task_c_cb(bb_task_t *task, void *userdata)
{
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 3;

    printf("Task C executed\n");

    bb_runtime_stop(ctx->runtime);

    bb_task_destroy(task);
}

static void task_b_cb(bb_task_t *task, void *userdata)
{
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 2;

    printf("Task B executed\n");

    bb_task_t *task_c = bb_task_create(task_c_cb, ctx);

    assert(task_c != NULL);

    assert(bb_runtime_schedule(ctx->runtime, task_c) == 0);

    bb_task_destroy(task);
}

static void task_a_cb(bb_task_t *task, void *userdata)
{
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 1;

    printf("Task A executed\n");

    bb_task_t *task_b = bb_task_create(task_b_cb, ctx);

    assert(task_b != NULL);

    assert(bb_runtime_schedule(ctx->runtime, task_b) == 0);

    bb_task_destroy(task);
}

void test_runtime_chain(void)
{
    bb_runtime_t *runtime = bb_runtime_create();

    assert(runtime != NULL);

    runtime_test_ctx_t ctx = {
        .runtime = runtime
    };

    bb_task_t *task_a = bb_task_create(task_a_cb, &ctx);

    assert(task_a != NULL);

    assert(bb_runtime_schedule(runtime, task_a) == 0);

    bb_runtime_run(runtime);

    // Validate execution chain

    assert(execution_index == 3);

    assert(execution_order[0] == 1);
    assert(execution_order[1] == 2);
    assert(execution_order[2] == 3);

    bb_runtime_destroy(runtime);
}

int main(void)
{
    printf("Starting runtime integration test...\n");
    test_runtime_chain();
    printf("Runtime integration test passed.\n");
    return 0;
}
