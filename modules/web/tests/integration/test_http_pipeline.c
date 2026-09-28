#include "blue-bird/web/server.h"
#include "blue-bird/web/client.h"
#include "blue-bird/utils/platform.h"

#include <stdio.h>
#include <string.h>
#include <blue-bird/error/assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

bb_runtime_t *server_runtime = NULL;

bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello, Blue-Bird :)");
    return BB_SUCCESS();
}

bb_error_t request_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *name = bb_request_get_param(req, "name");
    bb_response_set_header(res, "Content-Type", "text/plain");
    char msg[512];
    snprintf(msg, sizeof(msg), "name: %s", name);
    bb_response_set_body(res, msg);
    return BB_SUCCESS();
}

bb_error_t multi_request_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *p_1 = bb_request_get_param(req, "param_1");
    const char *p_2 = bb_request_get_param(req, "param_2");
    bb_response_set_header(res, "Content-Type", "text/plain");
    char msg[512];
    snprintf(msg, sizeof(msg), "%s and %s", p_1, p_2);
    bb_response_set_body(res, msg);
    return BB_SUCCESS();
}

bb_error_t request_query_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *value = bb_request_get_query_param(req, "val");
    if (!value)
    {
        bb_response_set_status(res, 400);
        return BB_SUCCESS();
    }
    bb_response_set_header(res, "Content-Type", "text/plain");
    char msg[512];
    snprintf(msg, sizeof(msg), "val: %s", value);
    bb_response_set_body(res, msg);
    return BB_SUCCESS();
}

bb_error_t request_multi_query_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *value_1 = bb_request_get_query_param(req, "val_1");
    const char *value_2 = bb_request_get_query_param(req, "val_2");
    if (!value_1 || !value_2)
    {
        bb_response_set_status(res, 400);
        return BB_SUCCESS();
    }
    bb_response_set_header(res, "Content-Type", "text/plain");
    char msg[512];
    snprintf(msg, sizeof(msg), "%s-%s", value_1, value_2);
    bb_response_set_body(res, msg);
    return BB_SUCCESS();
}

bb_error_t request_body_handler(bb_request_t *req, bb_response_t *res)
{
    bb_http_message_t *http_msg = bb_request_get_message(req);
    bb_response_set_header(res, "Content-Type", "text/plain");
    char *msg = malloc(bb_message_get_body_len(http_msg) + 10);
    snprintf(msg, (bb_message_get_body_len(http_msg) + 10), "body: %s", bb_message_get_body(http_msg) ? bb_message_get_body(http_msg) : "");
    bb_response_set_body(res, msg);
    free(msg);
    return BB_SUCCESS();
}

bb_error_t large_response_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    const size_t size = 1024 * 1024; // 1 MB
    char *body = malloc(size + 1);
    for (size_t i = 0; i < size; i++)
    {
        body[i] = 'A';
    }
    body[size] = '\0';
    bb_response_set_body(res, body);
    free(body);
    return BB_SUCCESS();
}

void *server(void* arg)
{
    (void) arg;
    server_runtime = bb_runtime_create();

    bb_server_t *server = bb_server_create_on_runtime(server_runtime, 8080);

    bb_server_add_route(server, "GET", "/", root_handler);
    bb_server_add_route(server, "GET", "/param/:name", request_param_handler);
    bb_server_add_route(server, "GET", "/param/:param_1/:param_2", multi_request_param_handler);
    bb_server_add_route(server, "GET", "/q_param", request_query_param_handler);
    bb_server_add_route(server, "GET", "/q_param/multi", request_multi_query_param_handler);
    bb_server_add_route(server, "GET", "/body", request_body_handler);
    bb_server_add_route(server, "GET", "/large_response", large_response_handler);
    bb_server_start(server);

    bb_runtime_run(server_runtime);

    bb_server_destroy(server);
    bb_runtime_destroy(server_runtime);
    return NULL;
}

void *concurrent_client(void *arg)
{
    (void)arg;

    for (int i = 0; i < 100; i++)
    {
        bb_client_t *client = bb_client_create();
        bb_request_t *req = bb_client_get_request(client);
        bb_response_t *res = bb_client_get_response(client);

        bb_request_set_method(req, "GET");
        bb_request_set_url(req, "http://127.0.0.1:8080/");
        bb_request_set_body(req, "");

        bb_error_t err = bb_client_execute(client);
        BB_ASSERT(err.code == 0);

        BB_ASSERT(bb_response_get_status(res) == 200);

        bb_client_destroy(client);
    }

    return NULL;
}

void test_root_req(void)
{
    printf("Testing root path...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Hello, Blue-Bird :)") == 0);

    bb_client_destroy(client);
}

void test_missing_path_req(void)
{
    printf("Testing missing path...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/missing_path";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_param_req(void)
{
    printf("Testing path with parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/param/my_name";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "name: my_name") == 0);

    bb_client_destroy(client);
}

void test_multi_param_req(void)
{
    printf("Testing path with multiple parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/param/hello/good_bye";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "hello and good_bye") == 0);

    bb_client_destroy(client);
}

void test_missing_param(void)
{
    printf("Testing missing route parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    char *url = "http://127.0.0.1:8080/param/";
    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_max_length_param(void)
{
    printf("Testing path with maximum length parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *body = "";
    char long_name[256 - sizeof("/param/") + 1];
    memset(long_name, 'a', sizeof(long_name)-1);
    long_name[sizeof(long_name)-1] = '\0';

    char url[6000];
    snprintf(url, sizeof(url), "http://127.0.0.1:8080/param/%s", long_name);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    char expected[6000];
    snprintf(expected, sizeof(expected), "name: %s", long_name);
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), expected) == 0);

    bb_client_destroy(client);
}

void test_over_sized_param(void)
{
    printf("Testing path with over sized parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *body = "";
    char long_name[257 - sizeof("/param/") + 1];
    memset(long_name, 'a', sizeof(long_name)-1);
    long_name[sizeof(long_name)-1] = '\0';

    char url[6000];
    snprintf(url, sizeof(url), "http://127.0.0.1:8080/param/%s", long_name);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 400);

    bb_client_destroy(client);
}

void test_query_param_req(void)
{
    printf("Testing path with Query parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/q_param?val=blue-bird";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "val: blue-bird") == 0);

    bb_client_destroy(client);
}

void test_encoded_query_param(void)
{
    printf("Testing encoded query parameter...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/q_param?val=blue%20bird%21");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "val: blue bird!") == 0);

    bb_client_destroy(client);
}

void test_multi_query_param_req(void)
{
    printf("Testing path with multiple Query parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/q_param/multi?val_2=bird&val_1=blue";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "blue-bird") == 0);

    bb_client_destroy(client);
}

void test_too_many_query_params(void)
{
    printf("Testing too many query parameters...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    char big_query[2048];
    strcpy(big_query, "http://127.0.0.1:8080/q_param?");
    for (int i = 0; i < 100; i++) {
        char frag[50];
        snprintf(frag, sizeof(frag), "val%d=%d&", i, i);
        strcat(big_query, frag);
    }

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, big_query);
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 400);

    bb_client_destroy(client);
}

void test_missing_query_param_req(void)
{
    printf("Testing path with missing Query parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/q_param";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 400);

    bb_client_destroy(client);
}

void test_duplicate_query_param(void)
{
    printf("Testing duplicate query parameter...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/q_param?val=blue&val=bird");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "val: blue") == 0); // Based on implementation, we expect first param

    bb_client_destroy(client);
}

void test_empty_query_value(void)
{
    printf("Testing empty query value...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/q_param?val=");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);
    bb_client_destroy(client);
}

void test_invalid_query_format(void)
{
    printf("Testing invalid query format (no '?')...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/q_paramval=blue-bird"); // missing '?'
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_req_body(void)
{
    printf("Testing request body...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/body";
    char *body = "BODY_CONTENT";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "body: BODY_CONTENT") == 0);

    bb_client_destroy(client);
}

void test_req_large_body(void)
{
    printf("Testing request with large body...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/body";
    const int size = 1024 * 1024; // 1 MB
    char *body = malloc(size + 100);
    for (int i = 0; i < size; i++)
    {
        body[i] = 'a' + (i % 26);
    }

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    char *expected_res = malloc(size + 100);
    snprintf(expected_res, size + 100, "body: %s", body);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), expected_res) == 0);

    free(body);
    free(expected_res);
    bb_client_destroy(client);
}

void test_large_response(void)
{
    printf("Testing large async response...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);


    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/large_response");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strlen(bb_response_get_body(res)) == 1024 * 1024);

    bb_client_destroy(client);
}

void test_empty_body_req(void)
{
    printf("Testing empty body...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/body");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "body: ") == 0);

    bb_client_destroy(client);
}

void test_encoded_body_req(void)
{
    printf("Testing encoded body...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---- build request ---- */
    char *url = "http://127.0.0.1:8080/body";
    char *body = "name=blue%20bird&msg=hello%21";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    /* optional but correct for encoded bodies */
    bb_request_set_header(req, "Content-Type", "application/x-www-form-urlencoded");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    /* ---- validate ---- */
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "body: name=blue bird&msg=hello!") == 0);

    bb_client_destroy(client);
}

void test_encoded_path_segment(void)
{
    printf("Testing URL encoded path segment...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/param/%62%6C%75%65"); // "blue"
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "name: blue") == 0);

    bb_client_destroy(client);
}

void test_invalid_body_encoding(void)
{
    printf("Testing invalid body encoding...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/body");
    bb_request_set_body(req, "name%GGbird"); // invalid percent encoding

    bb_request_set_header(req, "Content-Type", "application/x-www-form-urlencoded");
    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);

    bb_client_destroy(client);
}

void test_invalid_method(void)
{
    printf("Testing unsupported HTTP method...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "POST");
    bb_request_set_url(req, "http://127.0.0.1:8080/");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 405 || bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_trailing_slash(void)
{
    printf("Testing route with trailing slash...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/param/my_name/");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);

    BB_ASSERT(bb_response_get_status(res) == 200);

    bb_client_destroy(client);
}

void test_invalid_url_chars(void)
{
    printf("Testing path with invalid characters...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/param/<script>");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 400);

    bb_client_destroy(client);
}

void test_concurrent_clients(void)
{
    printf("Testing concurrent clients...\n");

    pthread_t threads[16];

    for (int i = 0; i < 16; i++)
    {
        pthread_create(&threads[i], NULL, concurrent_client, NULL);
    }

    for (int i = 0; i < 16; i++)
    {
        pthread_join(threads[i], NULL);
    }
}

void test_partial_request(void)
{
    printf("Testing partial request reads...\n");

    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    BB_ASSERT(rc == 0);

    send(fd, "GET / HTTP/1.1\r\n", 16, MSG_NOSIGNAL);

    bb_usleep(10000);

    send(fd, "Host: localhost\r\n", 17, MSG_NOSIGNAL);

    bb_usleep(10000);

    send(fd, "\r\n", 2, MSG_NOSIGNAL);

    char buffer[4096];

    ssize_t n = recv(fd, buffer, sizeof(buffer) - 1, 0);

    BB_ASSERT(n > 0);

    buffer[n] = '\0';

    BB_ASSERT(strstr(buffer, "Hello, Blue-Bird :)") != NULL);

    bb_socket_close(fd);
}

/* ------------------------------------------------------------------ */
/* Raw-socket helpers for HTTP parser tests                            */
/* ------------------------------------------------------------------ */

typedef enum
{
    RAW_PEER_CLOSED = 0, /* server closed (FIN or reset) */
    RAW_TIMED_OUT        /* server kept the connection open and went quiet */
} raw_result_t;

/*
 * Sends `req` verbatim over a fresh TCP connection (bypassing bb_client, which
 * would normalise or reject malformed requests) and collects whatever the
 * server sends back until it closes the connection or stays silent for ~2s.
 */
static raw_result_t raw_http_exchange(const void *req, size_t req_len, char *resp, size_t resp_cap)
{
    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    BB_ASSERT(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);

#ifdef _WIN32
    DWORD tv = 2000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    /* The server may reset the connection mid-send once it has rejected the
     * request, so a short/failed send is not an error here. */
    send(fd, req, req_len, MSG_NOSIGNAL);

    size_t total = 0;
    raw_result_t result = RAW_PEER_CLOSED;

    for (;;)
    {
        if (total + 1 >= resp_cap)
            break;

        ssize_t n = recv(fd, resp + total, resp_cap - total - 1, 0);
        if (n > 0)
        {
            total += (size_t)n;
            continue;
        }
        if (n == 0)
            break;

#ifdef _WIN32
        if (WSAGetLastError() == WSAETIMEDOUT)
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
            result = RAW_TIMED_OUT;
        break; /* timeout or connection reset */
    }

    resp[total] = '\0';
    bb_socket_close(fd);
    return result;
}

/* A plain request must still succeed: proves the server survived whatever
 * malformed input the previous test threw at it. */
static void assert_server_healthy(void)
{
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Hello, Blue-Bird :)") == 0);

    bb_client_destroy(client);
}

/* Every payload must be rejected by the parser: no 2xx response is served and
 * the server drops the connection instead of waiting for more bytes. */
static void assert_all_rejected(const char *const *payloads, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        char resp[4096];
        raw_result_t r = raw_http_exchange(payloads[i], strlen(payloads[i]), resp, sizeof(resp));

        if (r != RAW_PEER_CLOSED || strstr(resp, " 200 ") != NULL)
        {
            fprintf(stderr, "payload #%zu was not rejected:\n%s\n", i, payloads[i]);
        }
        BB_ASSERT(r == RAW_PEER_CLOSED);
        BB_ASSERT(strstr(resp, " 200 ") == NULL);
        BB_ASSERT(strstr(resp, "Hello, Blue-Bird") == NULL);
    }
    assert_server_healthy();
}

void test_parser_malformed_request_line(void)
{
    printf("Testing malformed request lines are rejected...\n");

    static const char *const payloads[] = {
        "GET /\r\nHost: localhost\r\n\r\n",                          /* missing version */
        "GET / HTTP/1.x\r\nHost: localhost\r\n\r\n",                 /* bad version */
        "GET / HTTP/11.1\r\nHost: localhost\r\n\r\n",                /* bad version */
        "GE(T / HTTP/1.1\r\nHost: localhost\r\n\r\n",                /* non-token method */
        "GET http://evil.example/ HTTP/1.1\r\nHost: localhost\r\n\r\n", /* not origin-form */
        "GET /#frag HTTP/1.1\r\nHost: localhost\r\n\r\n",            /* fragment in target */
        "GET / HTTP/1.1\nHost: localhost\n\n",                       /* bare LF */
        "\r\n",                                                      /* empty request line */
    };

    assert_all_rejected(payloads, sizeof(payloads) / sizeof(payloads[0]));
}

void test_parser_path_traversal_targets(void)
{
    printf("Testing traversal and unsafe encoded request targets are rejected...\n");

    static const char *const payloads[] = {
        "GET /../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n",
        "GET /param/../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n",
        "GET /param/%2e%2e/secret HTTP/1.1\r\nHost: localhost\r\n\r\n",
        "GET /param/.%2E/secret HTTP/1.1\r\nHost: localhost\r\n\r\n",
        "GET /param/a%2Fb HTTP/1.1\r\nHost: localhost\r\n\r\n",      /* encoded slash */
        "GET /param/a%5Cb HTTP/1.1\r\nHost: localhost\r\n\r\n",      /* encoded backslash */
        "GET /param/a\\b HTTP/1.1\r\nHost: localhost\r\n\r\n",       /* literal backslash */
        "GET /param/%00 HTTP/1.1\r\nHost: localhost\r\n\r\n",        /* encoded NUL */
        "GET /q_param?val=%0d%0aInjected:1 HTTP/1.1\r\nHost: localhost\r\n\r\n", /* CRLF in query */
        "GET /param/%zz HTTP/1.1\r\nHost: localhost\r\n\r\n",        /* malformed escape */
        "GET /param/% HTTP/1.1\r\nHost: localhost\r\n\r\n",          /* truncated escape */
    };

    assert_all_rejected(payloads, sizeof(payloads) / sizeof(payloads[0]));
}

void test_parser_ambiguous_framing(void)
{
    printf("Testing ambiguous body framing (request smuggling vectors) is rejected...\n");

    static const char *const payloads[] = {
        /* Content-Length + Transfer-Encoding */
        "GET /body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        /* duplicate Content-Length, even when equal */
        "GET /body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n"
        "Content-Length: 5\r\n\r\nhello",
        /* non-decimal / signed / overflowing Content-Length */
        "GET /body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5abc\r\n\r\nhello",
        "GET /body HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n",
        "GET /body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 99999999999999999999999\r\n\r\n",
        /* Content-Length above the parser's body limit */
        "GET /body HTTP/1.1\r\nHost: localhost\r\nContent-Length: 999999999\r\n\r\n",
        /* unsupported transfer coding */
        "GET /body HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\n",
        /* obsolete line folding */
        "GET / HTTP/1.1\r\nHost: localhost\r\nX-Folded: a\r\n b\r\n\r\n",
        /* header without a colon / with an invalid name */
        "GET / HTTP/1.1\r\nHost localhost\r\n\r\n",
        "GET / HTTP/1.1\r\nBad Name: x\r\n\r\n",
        /* bad chunk size */
        "GET /body HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nZZ\r\nhello\r\n0\r\n\r\n",
    };

    assert_all_rejected(payloads, sizeof(payloads) / sizeof(payloads[0]));
}

void test_parser_header_limits(void)
{
    printf("Testing oversized request line and header limits are rejected...\n");

    /* request-target longer than BB_HTTP_MAX_REQUEST_LINE (8192) */
    size_t long_len = 9000;
    char *long_target = malloc(long_len + 64);
    BB_ASSERT(long_target != NULL);
    int off = snprintf(long_target, 32, "GET /");
    memset(long_target + off, 'a', long_len);
    off += (int)long_len;
    off += snprintf(long_target + off, 64, " HTTP/1.1\r\nHost: localhost\r\n\r\n");

    char resp[4096];
    raw_result_t r = raw_http_exchange(long_target, (size_t)off, resp, sizeof(resp));
    BB_ASSERT(r == RAW_PEER_CLOSED);
    BB_ASSERT(strstr(resp, " 200 ") == NULL);
    free(long_target);

    /* a single header line larger than BB_HTTP_MAX_HEADER_SIZE (32768) */
    size_t hdr_len = 40000;
    char *big_header = malloc(hdr_len + 128);
    BB_ASSERT(big_header != NULL);
    off = snprintf(big_header, 64, "GET / HTTP/1.1\r\nHost: localhost\r\nX-Big: ");
    memset(big_header + off, 'b', hdr_len);
    off += (int)hdr_len;
    off += snprintf(big_header + off, 16, "\r\n\r\n");

    r = raw_http_exchange(big_header, (size_t)off, resp, sizeof(resp));
    BB_ASSERT(r == RAW_PEER_CLOSED);
    BB_ASSERT(strstr(resp, " 200 ") == NULL);
    free(big_header);

    /* more than BB_HTTP_MAX_HEADER_COUNT (100) header fields */
    char *many = malloc(8192);
    BB_ASSERT(many != NULL);
    off = snprintf(many, 64, "GET / HTTP/1.1\r\nHost: localhost\r\n");
    for (int i = 0; i < 150; i++)
    {
        off += snprintf(many + off, 64, "X-H%d: v\r\n", i);
    }
    off += snprintf(many + off, 8, "\r\n");

    r = raw_http_exchange(many, (size_t)off, resp, sizeof(resp));
    BB_ASSERT(r == RAW_PEER_CLOSED);
    BB_ASSERT(strstr(resp, " 200 ") == NULL);
    free(many);

    assert_server_healthy();
}

void test_parser_chunked_body(void)
{
    printf("Testing chunked request body is decoded...\n");

    const char *req =
        "GET /body HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "6\r\n world\r\n"
        "0\r\n"
        "\r\n";

    char resp[4096];
    (void)raw_http_exchange(req, strlen(req), resp, sizeof(resp));

    /* keep-alive means the server may not close; only the content matters */
    BB_ASSERT(strstr(resp, " 200 ") != NULL);
    BB_ASSERT(strstr(resp, "body: hello world") != NULL);
}

void test_many_requests(void)
{
    printf("Testing many sequential requests...\n");

    for (int i = 0; i < 5000; i++)
    {
        bb_client_t *client = bb_client_create();
        bb_request_t *req = bb_client_get_request(client);
        bb_response_t *res = bb_client_get_response(client);

        bb_request_set_method(req, "GET");
        bb_request_set_url(req, "http://127.0.0.1:8080/");
        bb_request_set_body(req, "");

        bb_error_t err = bb_client_execute(client);
        BB_ASSERT(err.code == 0);

        BB_ASSERT(bb_response_get_status(res) == 200);

        bb_client_destroy(client);
    }
}

void test_client_reset_reuse(void)
{
    printf("Testing client reset and reuse...\n");

    bb_client_t *client = bb_client_create();

    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    /* ---------- Request #1 ---------- */

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);

    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Hello, Blue-Bird :)") == 0);

    /* ---------- Reset ---------- */

    bb_client_reset(client);

    /* ---------- Request #2 ---------- */

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/param/bluebird");
    bb_request_set_body(req, "");

    err = bb_client_execute(client);

    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "name: bluebird") == 0);

    bb_client_destroy(client);
}

void test_client_reset_different_host(void)
{
    printf("Testing client reset with different URL authority...\n");

    bb_client_t *client = bb_client_create();

    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://127.0.0.1:8080/");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client);

    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 200);

    bb_client_reset(client);

    /*
     * Same server, different authority string.
     * Exercises URL re-parsing and reconnect logic.
     */
    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "http://localhost:8080/");
    bb_request_set_body(req, "");

    err = bb_client_execute(client);

    BB_ASSERT(err.code == 0);
    BB_ASSERT(bb_response_get_status(res) == 200);

    bb_client_destroy(client);
}

void test_client_multiple_reuse(void)
{
    printf("Testing repeated client reuse...\n");

    bb_client_t *client = bb_client_create();

    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    for (int i = 0; i < 1000; i++)
    {
        bb_client_reset(client);

        bb_request_set_method(req, "GET");
        bb_request_set_url(req, "http://127.0.0.1:8080/");
        bb_request_set_body(req, "");

        bb_error_t err = bb_client_execute(client);

        BB_ASSERT(err.code == 0);
        BB_ASSERT(bb_response_get_status(res) == 200);
    }

    bb_client_destroy(client);
}

static volatile int async_done = 0;

static void async_get_cb(bb_client_t *client, bb_error_t err, void *userdata)
{
    (void)userdata;

    BB_ASSERT(err.code == 0);

    bb_response_t *res = bb_client_get_response(client);

    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Hello, Blue-Bird :)") == 0);

    async_done = 1;
}

void test_async_get(void)
{
    printf("Testing async GET...\n");

    async_done = 0;

    bb_client_t *client = bb_client_create();

    bb_client_get_async(client, "http://127.0.0.1:8080/", async_get_cb, NULL);

    bb_runtime_set_running(bb_runtime_default());
    while (!async_done)
    {
        bb_runtime_tick(bb_runtime_default());
    }

    bb_client_destroy(client);
}

static volatile int async_completed = 0;

static void async_many_cb(bb_client_t *client, bb_error_t err, void *userdata)
{
    (void)userdata;

    BB_ASSERT(err.code == 0);

    bb_response_t *res = bb_client_get_response(client);

    BB_ASSERT(bb_response_get_status(res) == 200);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Hello, Blue-Bird :)") == 0);

    async_completed++;
}

void test_async_many_clients(void)
{
    printf("Testing many async clients...\n");
    const int count = 100;
    async_completed = 0;
    bb_client_t **client_list = malloc(sizeof(bb_client_t *) * (size_t)count);
    for (int i = 0; i < count; i++)
    {
        client_list[i] = bb_client_create();
        bb_client_get_async(client_list[i], "http://127.0.0.1:8080/", async_many_cb, NULL);
    }
    while (async_completed < count)
    {
        bb_runtime_tick(bb_runtime_default());
    }
    for (int i = 0; i < count; i++)
    {
        bb_client_destroy(client_list[i]);
    }
    free(client_list);
}

int main(void)
{
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, server, NULL) != 0)
    {
        fprintf(stderr, "Error creating server thread\n");
        return 1;
    }

    while (!bb_runtime_is_running(server_runtime))
    {
        bb_usleep(10000);
    }

    test_root_req();
    test_missing_path_req();
    test_param_req();
    test_multi_param_req();
    test_missing_param();
    test_max_length_param();
    test_over_sized_param();
    test_query_param_req();
    test_encoded_query_param();
    test_multi_query_param_req();
    test_too_many_query_params();
    test_missing_query_param_req();
    test_duplicate_query_param();
    test_empty_query_value();
    test_invalid_query_format();
    test_req_body();
    test_req_large_body();
    test_large_response();
    test_empty_body_req();
    test_encoded_body_req();
    test_encoded_path_segment();
    test_invalid_body_encoding();
    test_invalid_method();
    test_trailing_slash();
    test_invalid_url_chars();
    test_concurrent_clients();
    test_partial_request();
    test_parser_malformed_request_line();
    test_parser_path_traversal_targets();
    test_parser_ambiguous_framing();
    test_parser_header_limits();
    test_parser_chunked_body();
    test_many_requests();
    test_client_reset_reuse();
    test_client_reset_different_host();
    test_client_multiple_reuse();
    test_async_get();
    test_async_many_clients();

    printf("HTTP client and server integration tests passed.\n");

    bb_runtime_stop(server_runtime);
    pthread_join(thread_id, NULL);

    bb_runtime_destroy(bb_runtime_default());
    return 0;
}
