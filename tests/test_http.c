#include "core/http.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

void test_response_basic()
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

void test_response_multiple_headers()
{
    Response res;
    char buffer[1024];

    init_response(&res);
    set_status(&res, 201);
    set_header(&res, "Content-Type", "application/json");
    set_header(&res, "X-Custom", "Blue-Bird");
    set_body(&res, "{\"ok\":true}");

    int len = serialize_response(&res, buffer, sizeof(buffer));
    
    assert(strstr(buffer, "HTTP/1.1 201 Created") != NULL);
    assert(strstr(buffer, "Content-Type: application/json") != NULL);
    assert(strstr(buffer, "X-Custom: Blue-Bird") != NULL);
    assert(strstr(buffer, "Content-Length: 11") != NULL);
    assert(strstr(buffer, "{\"ok\":true}") != NULL);

    destroy_response(&res);
}

void test_response_empty_body()
{
    Response res;
    char buffer[1024];

    init_response(&res);
    set_status(&res, 204);

    int len = serialize_response(&res, buffer, sizeof(buffer));
   
    assert(strstr(buffer, "HTTP/1.1 204 No Content") != NULL);
    assert(strstr(buffer, "Content-Length: 0") != NULL);

    destroy_response(&res);
}

void test_response_large_body()
{
    Response res;
    char buffer[65536];

    size_t large_size = 50 * 1000 + 1;
    char *large_body = malloc(large_size); // 50 KB

    for (int i = 0; i < large_size - 1; i++)
        large_body[i] = 'A';
    large_body[large_size - 1] = '\0';

    init_response(&res);
    set_status(&res, 200);
    set_header(&res, "Content-Type", "text/plain");
    set_body(&res, large_body);

    int len = serialize_response(&res, buffer, sizeof(buffer));

    assert(strstr(buffer, "Content-Length: 50000") != NULL);
    assert(buffer[len - 1] == 'A');

    destroy_response(&res);
}

int main()
{
    printf("Running Request tests...\n");
    test_request();
    printf("Request tests passed.\n");
    
    printf("Running Response tests...\n");
    test_response_basic();
    test_response_multiple_headers();
    test_response_empty_body();
    test_response_large_body();
    printf("Response tests passed.\n");
    
    printf("All tests passed.\n");
    return 0;
}
