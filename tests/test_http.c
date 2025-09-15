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
    const char *raw = "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int res = parse_request(raw, &req);
    assert(res == 0);
    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/hello") == 0);
    assert(strcmp(req.version, "HTTP/1.1") == 0);

    const char *bad_request = "INVALID REQUEST";
    res = parse_request(bad_request, &req);
    assert(res != 0);

}

void test_response_creation()
{
    Response res;
    char buffer[1024];

    init_response(&res);
    set_status(&res, 200);
    set_header(&res, "Content-Type", "text/plain");
    set_body(&res, "Hello there!");

    int len = serialize_response(&res, buffer, sizeof(buffer));
    
    assert(strstr(buffer, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(buffer, "Content-Type: text/plain") != NULL);
    assert(strstr(buffer, "Content-Length: 12") != NULL);
    assert(strstr(buffer, "Hello there!") != NULL);

    destroy_response(&res);
}

int main()
{
    printf("Running Request tests...\n");
    test_request_creation();
    printf("Request tests passed.\n");
    
    printf("Running Response tests...\n");
    test_response_creation();
    printf("Response tests passed.\n");
    
    printf("All tests passed.\n");
    return 0;
}
