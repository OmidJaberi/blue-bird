#ifndef BB_WEB_EXECUTOR_H
#define BB_WEB_EXECUTOR_H

#ifdef __cplusplus
extern "C" {
#endif


#include "http/request.h"
#include "http/response.h"
#include "router.h"

typedef struct bb_web_executor bb_web_executor_t;

bb_web_executor_t *bb_web_executor_create(
    void
);

void bb_web_executor_destroy(
    bb_web_executor_t *executor
);

bb_error_t bb_web_executor_execute(
    bb_web_executor_t *executor,
    bb_route_handler_cb handler,
    bb_request_t *req,
    bb_response_t *res
);


#ifdef __cplusplus
}
#endif

#endif
