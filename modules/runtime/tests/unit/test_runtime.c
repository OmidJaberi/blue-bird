#include <assert.h>
#include <stdio.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/task.h"

static int executed = 0;

static void runtime_task_cb(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_runtime_t *runtime = userdata;

    executed = 1;

    bb_runtime_stop(runtime);
}

void test_runtime(void)
{
    bb_runtime_t *runtime = bb_runtime_create();

    assert(runtime != NULL);

    assert(bb_runtime_schedule(runtime, runtime_task_cb, runtime) != NULL);

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
