#include <assert.h>
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
    bb_task_t *task = bb_task_create(test_task_cb, NULL);

    assert(task != NULL);

    bb_task_execute(task);

    assert(executed == 1);
}

int main(void)
{
    printf("Running Task tests...\n");
    test_task();
    printf("Task tests passed.\n");
    return 0;
}
