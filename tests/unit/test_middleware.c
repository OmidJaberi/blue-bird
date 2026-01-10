#include "core/middleware.h"
#include "core/http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test middleware order
static int call_order[3];
static int call_index = 0;

BBError mw1(request_t *req, response_t *res)
{
    call_order[call_index++] = 1;
    return BB_SUCCESS();
}

BBError mw2(request_t *req, response_t *res)
{
    call_order[call_index++] = 2;
    return BB_SUCCESS();
}

BBError mw3(request_t *req, response_t *res)
{
    call_order[call_index++] = 3;
    return BB_SUCCESS();
}

// Test stop middleware
BBError mw_stop(request_t *req, response_t *res)
{
    set_response_status(res, 403);
    set_response_body(res, "Forbidden");
    return BB_ERROR(BB_ERR_BAD_REQUEST, "Forbidden"); // stop chain
}

void test_middleware_order()
{
    MiddlewareList mw_list;
    init_middleware_list(&mw_list);
    response_t res;
    request_t req;
    call_index = 0;

    init_response(&res);
    append_to_middleware_list(&mw_list, mw1);
    append_to_middleware_list(&mw_list, mw2);
    append_to_middleware_list(&mw_list, mw3);

    BBError result = run_middleware(&mw_list, &req, &res);
    assert(!BB_FAILED(result));
    assert(call_order[0] == 1);
    assert(call_order[1] == 2);
    assert(call_order[2] == 3);
    
    destroy_middleware_list(&mw_list);
    destroy_response(&res);
}

void test_middleware_stop()
{
    MiddlewareList mw_list;
    init_middleware_list(&mw_list);
    response_t res;
    request_t req;

    init_response(&res);
    append_to_middleware_list(&mw_list, mw_stop);

    BBError result = run_middleware(&mw_list, &req, &res);
    assert(BB_FAILED(result));
    assert(res.status_code == 403);
    assert(strcmp(res.msg.body, "Forbidden") == 0);
    
    destroy_middleware_list(&mw_list);
    destroy_response(&res);
}

int main()
{
    test_middleware_order();
    test_middleware_stop();
    printf("All middleware tests passed.\n");
    return 0;
}
