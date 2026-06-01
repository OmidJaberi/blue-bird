#include "blue-bird/web/middleware.h"
#include "blue-bird/web/http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test middleware order
static int call_order[3];
static int call_index = 0;

bb_error_t mw1(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    (void) res;
    call_order[call_index++] = 1;
    return BB_SUCCESS();
}

bb_error_t mw2(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    (void) res;
    call_order[call_index++] = 2;
    return BB_SUCCESS();
}

bb_error_t mw3(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    (void) res;
    call_order[call_index++] = 3;
    return BB_SUCCESS();
}

// Test stop middleware
bb_error_t mw_stop(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_status(res, 403);
    bb_response_set_body(res, "Forbidden");
    return BB_ERROR(BB_ERR_BAD_REQUEST, "Forbidden"); // stop chain
}

void test_middleware_order(void)
{
    printf("Testing Middleware order...\n");
    bb_middleware_list_t *mw_list = bb_middleware_list_create();
    bb_response_t res;
    bb_request_t req;
    call_index = 0;

    bb_response_init(&res);
    bb_middleware_list_append(mw_list, mw1);
    bb_middleware_list_append(mw_list, mw2);
    bb_middleware_list_append(mw_list, mw3);

    bb_error_t result = bb_middleware_list_run(mw_list, &req, &res);
    assert(!BB_FAILED(result));
    assert(call_order[0] == 1);
    assert(call_order[1] == 2);
    assert(call_order[2] == 3);
    
    bb_middleware_list_destroy(mw_list);
    bb_response_destroy(&res);
}

void test_middleware_stop(void)
{
    printf("Testing Middleware stop...\n");
    bb_middleware_list_t *mw_list = bb_middleware_list_create();
    bb_response_t res;
    bb_request_t req;

    bb_response_init(&res);
    bb_middleware_list_append(mw_list, mw_stop);

    bb_error_t result = bb_middleware_list_run(mw_list, &req, &res);
    assert(BB_FAILED(result));
    assert(res.status_code == 403);
    assert(strcmp(res.msg.body, "Forbidden") == 0);
    
    bb_middleware_list_destroy(mw_list);
    bb_response_destroy(&res);
}

int main(void)
{
    test_middleware_order();
    test_middleware_stop();
    printf("All middleware tests passed.\n");
    return 0;
}
