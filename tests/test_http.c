#include "core/http.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_request_creation()
{
    Request req;
    req.method[0] = '\0';
    req.path[0] = '\0';
    req.version[0] = '\0';
    int res = parse_request("GET /hello HTTP/1.1\r\nHost: localhost\r\n", &req);
    assert(res == 0);
    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/hello") == 0);
    assert(strcmp(req.version, "HTTP/1.1") == 0);
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
