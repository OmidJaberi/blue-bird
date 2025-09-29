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

void test_parse_get_request()
{
    const char *raw =
        "GET /hello HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: TestClient\r\n"
        "\r\n";
    
    Request req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/hello") == 0);
    assert(strcmp(req.version, "HTTP/1.1") == 0);
   
    assert(strcmp(get_header(&req, "Host"), "localhost:8080") == 0);
    assert(strcmp(get_header(&req, "User-Agent"), "TestClient") == 0);
    
    assert(req.body == NULL);
    assert(req.body_len == 0);

    destroy_request(&req);
}

void test_parse_post_request_with_body()
{
    const char *raw =
        "POST /submit HTTP/1.1\r\n"
        "Host: localhsot:8080\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Hello World";
    
    Request req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.method, "POST") == 0);
    assert(strcmp(req.path, "/submit") == 0);
    
    assert(strcmp(get_header(&req, "Content-Type"), "text/plain") == 0);
    assert(strcmp(get_header(&req, "Content-Length"), "11") == 0);

    assert(req.body != NULL);
    assert(strcmp(req.body, "Hello World") == 0);
    assert(req.body_len == 11);

    destroy_request(&req);
}

void test_parse_request_with_no_headers()
{
    const char *raw =
        "GET /ping HTTP/1.0\r\n"
        "\r\n";
    
    Request req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/ping") == 0);
    assert(strcmp(req.version, "HTTP/1.0") == 0);
    
    assert(req.header_count == 0);
    assert(get_header(&req, "Host") == NULL);

    destroy_request(&req);
}

int main()
{
    printf("Running Request tests...\n");
    test_request();
    test_parse_get_request();
    test_parse_post_request_with_body();
    test_parse_request_with_no_headers();
    printf("All tests passed.\n");
    return 0;
}
