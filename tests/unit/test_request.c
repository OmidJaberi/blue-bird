#include "core/http/request.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_request()
{
    request_t req;

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
    
    request_t req;
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
    
    request_t req;
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
    
    request_t req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/ping") == 0);
    assert(strcmp(req.version, "HTTP/1.0") == 0);
    
    assert(req.header_count == 0);
    assert(get_header(&req, "Host") == NULL);

    destroy_request(&req);
}

void test_malformed_request()
{
    const char *raw = "GET /hello\r\n\r\n";
    
    request_t req;
    int result = parse_request(raw, &req);
    assert(result == -1);
}

void test_request_with_invalid_header()
{
    const char *raw =
        "GET /ping HTTP/1.1\r\n"
        "Host localhost\r\n"
        "\r\n";
    
    request_t req;
    int result = parse_request(raw, &req);
    assert(result == -1);
}

void test_request_with_query_params()
{
    const char *raw =
        "GET /search?q=bluebird&limit=10 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    request_t req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/search") == 0);
    assert(strcmp(req.version, "HTTP/1.1") == 0);

    assert(req.query_count == 2);
    assert(strcmp(get_query_param(&req, "q"), "bluebird") == 0);
    assert(strcmp(get_query_param(&req, "limit"), "10") == 0);

    destroy_request(&req);
}

void test_request_with_empty_query_value()
{
    const char *raw =
        "GET /filter?enabled=&sort=asc HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    request_t req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.path, "/filter") == 0);

    assert(req.query_count == 2);
    assert(strcmp(get_query_param(&req, "enabled"), "") == 0);
    assert(strcmp(get_query_param(&req, "sort"), "asc") == 0);

    destroy_request(&req);
}

void test_request_with_no_query_value()
{
    const char *raw =
        "GET /items?flag HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    request_t req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.path, "/items") == 0);

    assert(req.query_count == 1);
    assert(strcmp(get_query_param(&req, "flag"), "") == 0);

    destroy_request(&req);
}

void test_request_max_query_params()
{
    const char *raw =
        "GET /many?"
        "k1=v1&k2=v2&k3=v3&k4=v4&k5=v5&"
        "k6=v6&k7=v7&k8=v8&k9=v9&k10=v10&k11=v11&k12=v12 "
        "HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    request_t req;
    int result = parse_request(raw, &req);
    assert(result == 0);

    assert(strcmp(req.path, "/many") == 0);

    assert(req.query_count == MAX_QUERY_PARAMS);

    assert(strcmp(get_query_param(&req, "k1"), "v1") == 0);
    assert(strcmp(get_query_param(&req, "k10"), "v10") == 0);

    assert(get_query_param(&req, "k11") == NULL);
    assert(get_query_param(&req, "k12") == NULL);

    destroy_request(&req);
}

int main()
{
    printf("Running Request tests...\n");
    test_request();
    test_parse_get_request();
    test_parse_post_request_with_body();
    test_parse_request_with_no_headers();
    test_malformed_request();
    test_request_with_invalid_header();
    test_request_with_query_params();
    test_request_with_empty_query_value();
    test_request_with_no_query_value();
    test_request_max_query_params();
    printf("All tests passed.\n");
    return 0;
}
