#include "core/http.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_request_creation()
{
    Request req;
    req.method = "GET";
    req.path = "/hello";
    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/hello") == 0);
}

void test_response_creation()
{
    Response res;
    res.status = 200;
    res.body = "OK";
    assert(res.status == 200);
    assert(strcmp(res.body, "OK") == 0);
}

int main()
{
    test_request_creation();
    test_response_creation();
    printf("All tests passed.\n");
    return 0;
}
