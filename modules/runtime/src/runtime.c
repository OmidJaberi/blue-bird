#include <stdlib.h>

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/loop.h"

struct bb_runtime {
    bb_loop_t *main_loop;
};

bb_runtime_t *bb_runtime_create(void)
{
    bb_runtime_t *runtime = malloc(sizeof(bb_runtime_t));

    if (!runtime)
    {
        return NULL;
    }

    runtime->main_loop = bb_loop_create();

    if (!runtime->main_loop)
    {
        free(runtime);
        return NULL;
    }

    return runtime;
}

void bb_runtime_destroy(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    bb_loop_destroy(runtime->main_loop);

    free(runtime);
}

void bb_runtime_run(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return;
    }

    bb_loop_run(runtime->main_loop);
}

bb_loop_t *bb_runtime_loop(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return NULL;
    }

    return runtime->main_loop;
}
