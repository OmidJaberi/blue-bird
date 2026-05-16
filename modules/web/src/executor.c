#include <stdlib.h>

#include "blue-bird/web/executor.h"

struct bb_web_executor {
    int reserved;
};

bb_web_executor_t *bb_web_executor_create(void)
{
    bb_web_executor_t *executor = malloc(sizeof(bb_web_executor_t));

    if (!executor)
    {
        return NULL;
    }

    executor->reserved = 0;

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

bb_error_t bb_web_executor_execute(
    bb_web_executor_t *executor,
    bb_route_handler_cb handler,
    bb_request_t *req,
    bb_response_t *res
)
{
    (void)executor;

    if (!handler || !req || !res)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "");
    }

    return handler(req, res);
}
