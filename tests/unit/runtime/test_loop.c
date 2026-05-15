#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "blue-bird/runtime/loop.h"
#include "blue-bird/runtime/task.h"

static int counter = 0;

static void increment_cb(bb_task_t *task, void *userdata)
{
    (void)task;
    (void)userdata;

    counter++;
}

void test_loop(void)
{
    bb_loop_t *loop = bb_loop_create();

    assert(loop != NULL);

    bb_task_t *t1 = bb_task_create(increment_cb, NULL);

    bb_task_t *t2 = bb_task_create(increment_cb, NULL);

    bb_loop_schedule(loop, t1);
    bb_loop_schedule(loop, t2);

    bb_loop_run(loop);

    assert(counter == 2);

    bb_task_destroy(t1);
    bb_task_destroy(t2);

    bb_loop_destroy(loop);
}

int main(void)
{
    printf("Running Loop tests...\n");
    test_loop();
    printf("Loop tests passed.\n");
    return 0;
}
