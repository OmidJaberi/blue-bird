#include <assert.h>
#include <stdio.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/task.h"

static int executed = 0;

static void runtime_task_cb(bb_task_t *task, void *userdata)
{
    bb_runtime_t *runtime = userdata;

    executed = 1;

    bb_runtime_stop(runtime);
    bb_task_destroy(task);
}

void test_runtime(void)
{
    bb_runtime_t *runtime = bb_runtime_create();

    assert(runtime != NULL);

    bb_task_t *task = bb_task_create(runtime_task_cb, runtime);
    assert(task != NULL);

    assert(bb_runtime_schedule(runtime, task) == 0);

    bb_runtime_run(runtime);

    assert(executed == 1);

    bb_runtime_destroy(runtime);
}

int main(void)
{
    printf("Running Runtime tests...\n");
    test_runtime();
    printf("Runtime tests passed.\n");
    return 0;
}
