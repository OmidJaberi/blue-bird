#include "http/server_request.h"
#include <blue-bird/error/assert.h>
#include <string.h>
#include <stdio.h>

void test_request(void)
{
    bb_server_request_t req;
    bb_server_request_init(&req);

    req.method[0] = '\0';
    req.path[0] = '\0';
    req.version[0] = '\0';
    const char *raw = "GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int res = bb_server_request_parse(raw, &req);
    BB_ASSERT(res == 0);
    BB_ASSERT(strcmp(req.method, "GET") == 0);
    BB_ASSERT(strcmp(req.path, "/hello") == 0);
    BB_ASSERT(strcmp(req.version, "HTTP/1.1") == 0);

    const char *bad_request = "INVALID REQUEST";
    res = bb_server_request_parse(bad_request, &req);
    BB_ASSERT(res != 0);

    bb_server_request_destroy(&req);
}

void test_parse_get_request(void)
{
    const char *raw =
        "GET /hello HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: TestClient\r\n"
        "\r\n";
    
    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == 0);

    BB_ASSERT(strcmp(req.method, "GET") == 0);
    BB_ASSERT(strcmp(req.path, "/hello") == 0);
    BB_ASSERT(strcmp(req.version, "HTTP/1.1") == 0);
   
    BB_ASSERT(strcmp(bb_message_get_header(req.msg, "Host"), "localhost:8080") == 0);
    BB_ASSERT(strcmp(bb_message_get_header(req.msg, "User-Agent"), "TestClient") == 0);
    
    BB_ASSERT(bb_message_get_body(req.msg) == NULL);
    BB_ASSERT(bb_message_get_body_len(req.msg) == 0);

    bb_server_request_destroy(&req);
}

void test_parse_post_request_with_body(void)
{
    const char *raw =
        "POST /submit HTTP/1.1\r\n"
        "Host: localhsot:8080\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Hello World";
    
    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == 0);

    BB_ASSERT(strcmp(req.method, "POST") == 0);
    BB_ASSERT(strcmp(req.path, "/submit") == 0);
    
    BB_ASSERT(strcmp(bb_message_get_header(req.msg, "Content-Type"), "text/plain") == 0);
    BB_ASSERT(strcmp(bb_message_get_header(req.msg, "Content-Length"), "11") == 0);

    BB_ASSERT(bb_message_get_body(req.msg) != NULL);
    BB_ASSERT(strcmp(bb_message_get_body(req.msg), "Hello World") == 0);
    BB_ASSERT(bb_message_get_body_len(req.msg) == 11);

    bb_server_request_destroy(&req);
}

void test_parse_server_request_with_no_headers(void)
{
    const char *raw =
        "GET /ping HTTP/1.0\r\n"
        "\r\n";
    
    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == 0);

    BB_ASSERT(strcmp(req.method, "GET") == 0);
    BB_ASSERT(strcmp(req.path, "/ping") == 0);
    BB_ASSERT(strcmp(req.version, "HTTP/1.0") == 0);
    
    BB_ASSERT(bb_message_get_header_count(req.msg) == 0);
    BB_ASSERT(bb_message_get_header(req.msg, "Host") == NULL);

    bb_server_request_destroy(&req);
}

void test_malformed_request(void)
{
    const char *raw = "GET /hello\r\n\r\n";
    
    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == -1);

    bb_server_request_destroy(&req);
}

void test_request_with_invalid_header(void)
{
    const char *raw =
        "GET /ping HTTP/1.1\r\n"
        "Host localhost\r\n"
        "\r\n";
    
    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == -1);

    bb_server_request_destroy(&req);
}

void test_request_with_query_params(void)
{
    const char *raw =
        "GET /search?q=bluebird&limit=10 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == 0);

    BB_ASSERT(strcmp(req.method, "GET") == 0);
    BB_ASSERT(strcmp(req.path, "/search") == 0);
    BB_ASSERT(strcmp(req.version, "HTTP/1.1") == 0);

    BB_ASSERT(req.query_count == 2);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "q"), "bluebird") == 0);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "limit"), "10") == 0);

    bb_server_request_destroy(&req);
}

void test_request_with_empty_query_value(void)
{
    const char *raw =
        "GET /filter?enabled=&sort=asc HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == 0);

    BB_ASSERT(strcmp(req.path, "/filter") == 0);

    BB_ASSERT(req.query_count == 2);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "enabled"), "") == 0);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "sort"), "asc") == 0);

    bb_server_request_destroy(&req);
}

void test_request_with_no_query_value(void)
{
    const char *raw =
        "GET /items?flag HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result == 0);

    BB_ASSERT(strcmp(req.path, "/items") == 0);

    BB_ASSERT(req.query_count == 1);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "flag"), "") == 0);

    bb_server_request_destroy(&req);
}

void test_request_max_query_params(void)
{
    const char *raw =
        "GET /many?"
        "k1=v1&k2=v2&k3=v3&k4=v4&k5=v5&"
        "k6=v6&k7=v7&k8=v8&k9=v9&k10=v10&k11=v11&k12=v12 "
        "HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);
    int result = bb_server_request_parse(raw, &req);
    BB_ASSERT(result != 0);
    bb_server_request_destroy(&req);
}

void test_query_percent_decoding(void)
{
    const char *raw =
        "GET /search?q=blue+bird&name=Alice%20Bob&literal=%26%3D HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);

    BB_ASSERT(bb_server_request_parse(raw, &req) == 0);
    BB_ASSERT(strcmp(req.path, "/search") == 0);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "q"), "blue bird") == 0);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "name"), "Alice Bob") == 0);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "literal"), "&=") == 0);

    bb_server_request_destroy(&req);
}

void test_encoded_question_mark_stays_in_path(void)
{
    const char *raw =
        "GET /a%3Fb?real=yes HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);

    BB_ASSERT(bb_server_request_parse(raw, &req) == 0);
    BB_ASSERT(strcmp(req.path, "/a?b") == 0);
    BB_ASSERT(strcmp(bb_server_request_get_query_param(&req, "real"), "yes") == 0);

    bb_server_request_destroy(&req);
}

void test_query_parameter_limit_rejected(void)
{
    const char *raw =
        "GET /many?"
        "k1=v&k2=v&k3=v&k4=v&k5=v&k6=v&k7=v&k8=v&k9=v&k10=v&k11=v "
        "HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);

    BB_ASSERT(bb_server_request_parse(raw, &req) != 0);
    bb_server_request_destroy(&req);
}

void test_query_empty_parameter_rejected(void)
{
    const char *targets[] = {
        "/search?a=1&&b=2",
        "/search?a=1&",
        "/search?&a=1",
        "/search?=value"
    };

    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
    {
        char raw[256];
        snprintf(raw, sizeof(raw), "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", targets[i]);

        bb_server_request_t req;
        bb_server_request_init(&req);
        BB_ASSERT(bb_server_request_parse(raw, &req) != 0);
        bb_server_request_destroy(&req);
    }
}

void test_query_component_limits_rejected(void)
{
    char key[MAX_QUERY_PARAM_KEY + 1];
    char value[MAX_QUERY_PARAM_VALUE + 1];
    memset(key, 'k', sizeof(key) - 1);
    memset(value, 'v', sizeof(value) - 1);
    key[sizeof(key) - 1] = '\0';
    value[sizeof(value) - 1] = '\0';

    char raw[1024];
    snprintf(raw, sizeof(raw), "GET /search?%s=x HTTP/1.1\r\n\r\n", key);

    bb_server_request_t req;
    bb_server_request_init(&req);
    BB_ASSERT(bb_server_request_parse(raw, &req) != 0);
    bb_server_request_destroy(&req);

    snprintf(raw, sizeof(raw), "GET /search?k=%s HTTP/1.1\r\n\r\n", value);
    bb_server_request_init(&req);
    BB_ASSERT(bb_server_request_parse(raw, &req) != 0);
    bb_server_request_destroy(&req);
}

void test_query_decoded_control_rejected(void)
{
    const char *raw =
        "GET /search?q=hello%00world HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    bb_server_request_t req;
    bb_server_request_init(&req);
    BB_ASSERT(bb_server_request_parse(raw, &req) != 0);
    bb_server_request_destroy(&req);
}


int main(void)
{
    printf("Running Request tests...\n");
    test_request();
    test_parse_get_request();
    test_parse_post_request_with_body();
    test_parse_server_request_with_no_headers();
    test_malformed_request();
    test_request_with_invalid_header();
    test_request_with_query_params();
    test_request_with_empty_query_value();
    test_request_with_no_query_value();
    test_request_max_query_params();
    test_query_percent_decoding();
    test_encoded_question_mark_stays_in_path();
    test_query_parameter_limit_rejected();
    test_query_empty_parameter_rejected();
    test_query_component_limits_rejected();
    test_query_decoded_control_rejected();
    printf("All tests passed.\n");
    return 0;
}
