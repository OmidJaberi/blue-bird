#include "core/request.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_request()
{
    Request req;

    req.method[0] = '\0';
    req.path[0] = '\0';
    req.version[0] = '\0';
    const char *raw = "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int res = parse_request(raw, &req);
    assert(res == 0);
    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/hello") == 0);
    assert(strcmp(req.version, "HTTP/1.1") == 0);

    const char *bad_request = "INVALID REQUEST";
    res = parse_request(bad_request, &req);
    assert(res != 0);

    destroy_request(&req);
}

int main()
{
    printf("Running Request tests...\n");
    test_request();
    printf("All tests passed.\n");
    return 0;
}
