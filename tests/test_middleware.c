#include "core/middleware.h"
#include "core/http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test middleware order
static int call_order[3];
static int call_index = 0;

int mw1(Request *req, Response *res)
{
    call_order[call_index++] = 1;
    return 0;
}

int mw2(Request *req, Response *res)
{
    call_order[call_index++] = 2;
    return 0;
}

int mw3(Request *req, Response *res)
{
    call_order[call_index++] = 3;
    return 0;
}

// Test stop middleware
int mw_stop(Request *req, Response *res)
{
    set_status(res, 403);
    set_body(res, "Forbidden");
    return 1; // stop chain
}

void test_middleware_order()
{
    MiddlewareList mw_list;
    init_middleware_list(&mw_list);
    Response res;
    Request req;
    call_index = 0;

    init_response(&res);
    append_to_middleware_list(&mw_list, mw1);
    append_to_middleware_list(&mw_list, mw2);
    append_to_middleware_list(&mw_list, mw3);

    int result = run_middleware(&mw_list, &req, &res);
    assert(result == 0);
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
    Response res;
    Request req;

    init_response(&res);
    append_to_middleware_list(&mw_list, mw_stop);

    int result = run_middleware(&mw_list, &req, &res);
    assert(result != 0);
    assert(res.status_code == 403);
    assert(strcmp(res.body, "Forbidden") == 0);
    
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
