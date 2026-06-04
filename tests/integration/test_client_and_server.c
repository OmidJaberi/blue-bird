#include "blue-bird/web/server.h"
#include "blue-bird/web/client.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <arpa/inet.h>

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
    sprintf(msg, "name: %s", name);
    bb_response_set_body(res, msg);
    return BB_SUCCESS();
}

bb_error_t multi_request_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *p_1 = bb_request_get_param(req, "param_1");
    const char *p_2 = bb_request_get_param(req, "param_2");
    bb_response_set_header(res, "Content-Type", "text/plain");
    char msg[512];
    sprintf(msg, "%s and %s", p_1, p_2);
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
    sprintf(msg, "val: %s", value);
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
    sprintf(msg, "%s-%s", value_1, value_2);
    bb_response_set_body(res, msg);
    return BB_SUCCESS();
}

bb_error_t request_body_handler(bb_request_t *req, bb_response_t *res)
{
    bb_http_message_t *http_msg = bb_request_get_message(req);
    bb_response_set_header(res, "Content-Type", "text/plain");
    char *msg = malloc(bb_message_get_body_len(http_msg) + 10);
    sprintf(msg, "body: %s", bb_message_get_body(http_msg) ? bb_message_get_body(http_msg) : "");
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
    bb_server_t *server = bb_server_create(8080);

    bb_server_add_route(server, "GET", "/", root_handler);
    bb_server_add_route(server, "GET", "/param/:name", request_param_handler);
    bb_server_add_route(server, "GET", "/param/:param_1/:param_2", multi_request_param_handler);
    bb_server_add_route(server, "GET", "/q_param", request_query_param_handler);
    bb_server_add_route(server, "GET", "/q_param/multi", request_multi_query_param_handler);
    bb_server_add_route(server, "GET", "/body", request_body_handler);
    bb_server_add_route(server, "GET", "/large_response", large_response_handler);
    bb_server_start(server);

    return NULL;
}

void *concurrent_client(void *arg)
{
    (void)arg;

    for (int i = 0; i < 100; i++)
    {
        bb_client_t *client = bb_client_create();
        bb_request_t *req = bb_client_request(client);
        bb_response_t *res = bb_client_response(client);

        bb_request_set_method(req, "GET");
        bb_request_set_url(req, "/");
        bb_request_set_body(req, "");

        bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
        assert(err.code == 0);

        assert(bb_response_get_status(res) == 200);

        bb_client_destroy(client);
    }

    return NULL;
}

void test_root_req(void)
{
    printf("Testing root path...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "Hello, Blue-Bird :)") == 0);

    bb_client_destroy(client);
}

void test_missing_path_req(void)
{
    printf("Testing missing path...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/missing_path";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_param_req(void)
{
    printf("Testing path with parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/param/my_name";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "name: my_name") == 0);

    bb_client_destroy(client);
}

void test_multi_param_req(void)
{
    printf("Testing path with multiple parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "param/hello/good_bye";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "hello and good_bye") == 0);

    bb_client_destroy(client);
}

void test_missing_param(void)
{
    printf("Testing missing route parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    char *url = "/param/";
    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_max_length_param(void)
{
    printf("Testing path with maxium length parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *body = "";
    char long_name[256 - sizeof("/param/") + 1];
    memset(long_name, 'a', sizeof(long_name)-1);
    long_name[sizeof(long_name)-1] = '\0';

    char url[6000];
    sprintf(url, "/param/%s", long_name);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    char expected[6000];
    sprintf(expected, "name: %s", long_name);
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), expected) == 0);

    bb_client_destroy(client);
}

void test_over_sized_param(void)
{
    printf("Testing path with over sized parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *body = "";
    char long_name[257 - sizeof("/param/") + 1];
    memset(long_name, 'a', sizeof(long_name)-1);
    long_name[sizeof(long_name)-1] = '\0';

    char url[6000];
    sprintf(url, "/param/%s", long_name);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 400);

    bb_client_destroy(client);
}

void test_query_param_req(void)
{
    printf("Testing path with Query parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/q_param?val=blue-bird";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "val: blue-bird") == 0);

    bb_client_destroy(client);
}

void test_encoded_query_param(void)
{
    printf("Testing encoded query parameter...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/q_param?val=blue%20bird%21");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "val: blue bird!") == 0);

    bb_client_destroy(client);
}

void test_multi_query_param_req(void)
{
    printf("Testing path with multiple Query parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/q_param/multi?val_2=bird&val_1=blue";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "blue-bird") == 0);

    bb_client_destroy(client);
}

void test_too_many_query_params(void)
{
    printf("Testing too many query parameters...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    char big_query[2048];
    strcpy(big_query, "/q_param?");
    for (int i = 0; i < 100; i++) {
        char frag[50];
        sprintf(frag, "val%d=%d&", i, i);
        strcat(big_query, frag);
    }

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, big_query);
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);
    assert(bb_response_get_status(res) == 400);

    bb_client_destroy(client);
}

void test_missing_query_param_req(void)
{
    printf("Testing path with missing Query parameter...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/q_param";
    char *body = "";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 400);

    bb_client_destroy(client);
}

void test_duplicate_query_param(void)
{
    printf("Testing duplicate query parameter...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/q_param?val=blue&val=bird");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "val: blue") == 0); // Based on implementation, we expect first param

    bb_client_destroy(client);
}

void test_empty_query_value(void)
{
    printf("Testing empty query value...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/q_param?val=");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);
    bb_client_destroy(client);
}

void test_invalid_query_format(void)
{
    printf("Testing invalid query format (no '?')...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/q_paramval=blue-bird"); // missing '?'
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);
    assert(bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_req_body(void)
{
    printf("Testing request body...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/body";
    char *body = "BODY_CONTENT";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "body: BODY_CONTENT") == 0);

    bb_client_destroy(client);
}

void test_req_large_body(void)
{
    printf("Testing request with large body...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/body";
    const int size = 1024 * 1024; // 1 MB
    char *body = malloc(size + 100);
    for (int i = 0; i < size; i++)
    {
        body[i] = 'a' + (i % 26);
    }

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    char *expected_res = malloc(size + 100);
    snprintf(expected_res, size + 100, "body: %s", body);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), expected_res) == 0);

    free(body);
    free(expected_res);
    bb_client_destroy(client);
}

void test_large_response(void)
{
    printf("Testing large async response...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);


    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/large_response");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);
    assert(strlen(bb_response_get_body(res)) == 1024 * 1024);

    bb_client_destroy(client);
}

void test_empty_body_req(void)
{
    printf("Testing empty body...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/body");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "body: ") == 0);

    bb_client_destroy(client);
}

void test_encoded_body_req(void)
{
    printf("Testing encoded body...\n");

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    /* ---- build request ---- */
    char *url = "/body";
    char *body = "name=blue%20bird&msg=hello%21";

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);
    bb_request_set_body(req, body);

    /* optional but correct for encoded bodies */
    bb_request_set_header(req, "Content-Type", "application/x-www-form-urlencoded");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- validate ---- */
    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "body: name=blue bird&msg=hello!") == 0);

    bb_client_destroy(client);
}

void test_encoded_path_segment(void)
{
    printf("Testing URL encoded path segment...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/param/%62%6C%75%65"); // "blue"
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);
    assert(strcmp(bb_response_get_body(res), "name: blue") == 0);

    bb_client_destroy(client);
}

void test_invalid_body_encoding(void)
{
    printf("Testing invalid body encoding...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/body");
    bb_request_set_body(req, "name%GGbird"); // invalid percent encoding

    bb_request_set_header(req, "Content-Type", "application/x-www-form-urlencoded");
    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);

    bb_client_destroy(client);
}

void test_invalid_method(void)
{
    printf("Testing unsupported HTTP method...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "POST");
    bb_request_set_url(req, "/");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 405 || bb_response_get_status(res) == 404);

    bb_client_destroy(client);
}

void test_trailing_slash(void)
{
    printf("Testing route with trailing slash...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/param/my_name/");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);

    assert(bb_response_get_status(res) == 200);

    bb_client_destroy(client);
}

void test_invalid_url_chars(void)
{
    printf("Testing path with invalid characters...\n");
    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_request(client);
    bb_response_t *res = bb_client_response(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, "/param/<script>");
    bb_request_set_body(req, "");

    bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
    assert(err.code == 0);
    assert(bb_response_get_status(res) == 400);

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

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(fd, (struct sockaddr *)&addr, sizeof(addr));

    send(fd, "GET / HTTP/1.1\r\n", 17, 0);

    usleep(10000);

    send(fd, "Host: localhost\r\n", 17, 0);

    usleep(10000);

    send(fd, "\r\n", 2, 0);

    char buffer[4096];

    ssize_t n = recv(fd, buffer, sizeof(buffer) - 1, 0);

    assert(n > 0);

    buffer[n] = '\0';

    assert(strstr(buffer, "Hello, Blue-Bird :)") != NULL);

    close(fd);
}

void test_many_requests(void)
{
    printf("Testing many sequential requests...\n");

    for (int i = 0; i < 5000; i++)
    {
        bb_client_t *client = bb_client_create();
        bb_request_t *req = bb_client_request(client);
        bb_response_t *res = bb_client_response(client);

        bb_request_set_method(req, "GET");
        bb_request_set_url(req, "/");
        bb_request_set_body(req, "");

        bb_error_t err = bb_client_execute(client, "127.0.0.1", 8080);
        assert(err.code == 0);

        assert(bb_response_get_status(res) == 200);

        bb_client_destroy(client);
    }
}

int main(void)
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
    test_many_requests();

    printf("HTTP client and server integration tests passed.\n");
    return 0;
}
