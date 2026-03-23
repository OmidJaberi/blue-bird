#include "core/server.h"
#include "core/client.h"
#include "core/http.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

BBError root_handler(request_t *req, response_t *res)
{
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Hello, Blue-Bird :)");
    return BB_SUCCESS();
}

BBError request_param_handler(request_t *req, response_t *res)
{
    const char *name = get_request_param(req, "name");
    set_response_header(res, "Content-Type", "text/plain");
    char msg[512];
    sprintf(msg, "name: %s", name);
    set_response_body(res, msg);
    return BB_SUCCESS();
}

BBError multi_request_param_handler(request_t *req, response_t *res)
{
    const char *p_1 = get_request_param(req, "param_1");
    const char *p_2 = get_request_param(req, "param_2");
    set_response_header(res, "Content-Type", "text/plain");
    char msg[512];
    sprintf(msg, "%s and %s", p_1, p_2);
    set_response_body(res, msg);
    return BB_SUCCESS();
}

BBError request_query_param_handler(request_t *req, response_t *res)
{
    const char *value = get_request_query_param(req, "val");
    if (!value)
    {
        set_response_status(res, 400);
        return BB_SUCCESS();
    }
    set_response_header(res, "Content-Type", "text/plain");
    char msg[512];
    sprintf(msg, "val: %s", value);
    set_response_body(res, msg);
    return BB_SUCCESS();
}

BBError request_multi_query_param_handler(request_t *req, response_t *res)
{
    const char *value_1 = get_request_query_param(req, "val_1");
    const char *value_2 = get_request_query_param(req, "val_2");
    if (!value_1 || !value_2)
    {
        set_response_status(res, 400);
        return BB_SUCCESS();
    }
    set_response_header(res, "Content-Type", "text/plain");
    char msg[512];
    sprintf(msg, "%s-%s", value_1, value_2);
    set_response_body(res, msg);
    return BB_SUCCESS();
}

BBError request_body_handler(request_t *req, response_t *res)
{
    http_message_t *http_msg = &GET_SERVER_REQUEST_MESSAGE(*req);
    set_response_header(res, "Content-Type", "text/plain");
    char msg[512];
    sprintf(msg, "body: %s", http_msg->body);
    set_response_body(res, msg);
    return BB_SUCCESS();
}

void *server(void* arg)
{
    bb_server_t server;

    init_server(&server, 8080);
    add_route(&server, "GET", "/", root_handler);
    add_route(&server, "GET", "/param/:name", request_param_handler);
    add_route(&server, "GET", "/param/:param_1/:param_2", multi_request_param_handler);
    add_route(&server, "GET", "/q_param", request_query_param_handler);
    add_route(&server, "GET", "/q_param/multi", request_multi_query_param_handler);
    add_route(&server, "GET", "/body", request_body_handler);
    start_server(&server);

    return NULL;
}

void client_request(request_t *req, response_t *res)
{
    bb_client_t client;

    /* ---- connect ---- */
    BBError err = http_client_connect(&client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- send ---- */
    err = http_client_send(&client, req);
    assert(err.code == 0);

    /* ---- receive ---- */
    err = http_client_receive(&client, res);
    assert(err.code == 0);

    /* ---- cleanup ---- */
    http_client_close(&client);
}

void test_root_req()
{
    printf("Testing root path...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/";
    const char *body = "";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "Hello, Blue-Bird :)") == 0);

    destroy_request(&req);
    destroy_response(&res);
}

void test_missing_path_req()
{
    printf("Testing missing path...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/missing_path";
    const char *body = "";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 404);

    destroy_request(&req);
    destroy_response(&res);
}

void test_param_req()
{
    printf("Testing path with Param...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/param/my_name";
    const char *body = "";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "name: my_name") == 0);

    destroy_request(&req);
    destroy_response(&res);
}

void test_multi_param_req()
{
    printf("Testing path with multiple Param...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "param/hello/good_bye";
    const char *body = "";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "hello and good_bye") == 0);

    destroy_request(&req);
    destroy_response(&res);
}

void test_query_param_req()
{
    printf("Testing path with Query Param...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/q_param?val=blue-bird";
    const char *body = "";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "val: blue-bird") == 0);

    destroy_request(&req);
    destroy_response(&res);
}

void test_multi_query_param_req()
{
    printf("Testing path with multiple Query Param...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/q_param/multi?val_2=bird&val_1=blue";
    const char *body = "";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "blue-bird") == 0);

    destroy_request(&req);
    destroy_response(&res);
}

void test_missing_query_param_req()
{
    printf("Testing path with missing Query Param...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/q_param";
    const char *body = "";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 400);

    destroy_request(&req);
    destroy_response(&res);
}

void test_req_body()
{
    printf("Testing request body...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/body";
    const char *body = "BODY_CONTENT";

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    /* ---- validate ---- */
    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "body: BODY_CONTENT") == 0);

    destroy_request(&req);
    destroy_response(&res);
}

void test_req_large_body()
{
    printf("Testing request with large body...\n");

    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    const char *url = "/body";
    const int size = 4000;
    char body[size + 100];
    for (int i = 0; i < size; i++)
    {
        body[i] = 'a' + (i % 26);
    }

    set_request_method(&req, "GET");
    set_request_url(&req, url);
    set_request_body(&req, body);

    client_request(&req, &res);

    char expected_res[size + 100];
    snprintf(expected_res, size + 100, "body: %s", body);

    /* ---- validate ---- */
    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, expected_res) == 0);

    destroy_request(&req);
    destroy_response(&res);
}

int main()
{
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, server, NULL) != 0)
    {
        fprintf(stderr, "Error creating server thread\n");
        return 1;
    }

    test_root_req();
    test_missing_path_req();
    test_param_req();
    test_multi_param_req();
    test_query_param_req();
    test_multi_query_param_req();
    test_missing_query_param_req();
    test_req_body();
    test_req_large_body();

    printf("HTTP client and server integration tests passed.\n");
    return 0;
}