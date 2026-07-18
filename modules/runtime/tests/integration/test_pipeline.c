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

    printf("Task C executed\n");

    bb_runtime_stop(ctx->runtime);
}

static void task_b_cb(bb_task_t *task, void *userdata)
{
    (void) task;
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 2;

    printf("Task B executed\n");

    BB_ASSERT(bb_runtime_schedule(ctx->runtime, task_c_cb, ctx) != NULL);
}

static void task_a_cb(bb_task_t *task, void *userdata)
{
    (void) task;
    runtime_test_ctx_t *ctx = userdata;

    execution_order[execution_index++] = 1;

    printf("Task A executed\n");

    BB_ASSERT(bb_runtime_schedule(ctx->runtime, task_b_cb, ctx) != NULL);
}

void test_runtime_chain(void)
{
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

int main(void)
{
    printf("Starting runtime integration test...\n");
    test_runtime_chain();
    printf("Runtime integration test passed.\n");
    return 0;
}
