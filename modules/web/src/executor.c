#include <stdlib.h>

#include "blue-bird/web/executor.h"
#include "blue-bird/runtime/runtime.h"
#include "blue-bird/runtime/loop.h"

struct bb_web_executor {
    bb_runtime_t *runtime;
    bb_loop_t *loop;
};

typedef struct {
    bb_http_handler_cb handler;

    bb_request_t *req;
    bb_response_t *res;

} _bb_web_execution_t;

bb_web_executor_t *bb_web_executor_create(void)
{
    bb_web_executor_t *executor = malloc(sizeof(bb_web_executor_t));

    if (!executor)
    {
        return NULL;
    }

    executor->runtime = bb_runtime_create();
    executor->loop = bb_loop_create();

    return executor;
}

void bb_web_executor_destroy(bb_web_executor_t *executor)
{
    if (!executor)
    {
        return;
    }

    free(executor);
}

static void _bb_web_execute_task_cb(bb_task_t *task, void *userdata)
{
    _bb_web_execution_t *execution = userdata;

    execution->handler(execution->req, execution->res);

    free(execution);

    bb_task_destroy(task);
}

bb_error_t bb_web_executor_execute(
    bb_web_executor_t *executor,
    bb_http_handler_cb handler,
    bb_request_t *req,
    bb_response_t *res
)
{
    if (!executor)
    {
        return handler(req, res);
    }

    _bb_web_execution_t *execution = malloc(sizeof(*execution));

    if (!execution)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Allocation failed.");
    }

    execution->handler = handler;
    execution->req = req;
    execution->res = res;

    bb_task_t *task = bb_task_create(_bb_web_execute_task_cb, execution);

    if (!task)
    {
        free(execution);
        return BB_ERROR(BB_ERR_INTERNAL, "Allocation failed.");
    }

    int rc = bb_loop_schedule(executor->loop, task);
    if (rc != 0)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to execute.");
    }
    return BB_SUCCESS();
}
