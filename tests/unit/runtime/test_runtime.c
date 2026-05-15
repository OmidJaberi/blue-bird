#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/task.h"

static int executed = 0;

static void runtime_task_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    executed = 1;
}

void test_runtime(void)
{
    bb_runtime_t *runtime = bb_runtime_create();

    assert(runtime != NULL);

    bb_task_t *task = bb_task_create(runtime_task_cb, NULL);

    bb_loop_schedule(bb_runtime_loop(runtime), task);

    bb_runtime_run(runtime);

    assert(executed == 1);

    bb_task_destroy(task);

    bb_runtime_destroy(runtime);
}

int main(void)
{
    printf("Running Runtime tests...\n");
    test_runtime();
    printf("Runtime tests passed.\n");
    return 0;
}
