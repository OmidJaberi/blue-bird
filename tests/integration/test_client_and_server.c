#include "core/server.h"
#include "core/client.h"
#include "core/http.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

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

void *server(void* arg)
{
    bb_server_t server;

    init_server(&server, 8080);
    add_route(&server, "GET", "/", root_handler);
    add_route(&server, "GET", "/q_param/:name", request_param_handler);
    start_server(&server);

    return NULL;
}

int client_root_req()
{
    bb_client_t client;
    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    set_request_method(&req, "GET");
    set_request_url(&req, "/");
    set_request_header(&req, "Host", "127.0.0.1");
    set_request_header(&req, "Connection", "close");

    /* ---- connect ---- */
    BBError err = http_client_connect(&client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- send ---- */
    err = http_client_send(&client, &req);
    assert(err.code == 0);

    /* ---- receive ---- */
    err = http_client_receive(&client, &res);
    assert(err.code == 0);

    /* ---- validate ---- */
    printf("Status: %d\n", res.status_code);
    printf("Body: %s\n", res.msg.body);

    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "Hello, Blue-Bird :)") == 0);

    /* ---- cleanup ---- */
    http_client_close(&client);
    destroy_request(&req);
    destroy_response(&res);

    printf("Root path client-server test passed...\n");
    return 0;
}

int client_param_req()
{
    bb_client_t client;
    request_t req;
    response_t res;

    /* ---- init ---- */
    init_request(&req);
    init_response(&res);

    /* ---- build request ---- */
    set_request_method(&req, "GET");
    set_request_url(&req, "/q_param/my_name");
    set_request_header(&req, "Host", "127.0.0.1");
    set_request_header(&req, "Connection", "close");

    /* ---- connect ---- */
    BBError err = http_client_connect(&client, "127.0.0.1", 8080);
    assert(err.code == 0);

    /* ---- send ---- */
    err = http_client_send(&client, &req);
    assert(err.code == 0);

    /* ---- receive ---- */
    err = http_client_receive(&client, &res);
    assert(err.code == 0);

    /* ---- validate ---- */
    printf("Status: %d\n", res.status_code);
    printf("Body: %s\n", res.msg.body);

    assert(res.status_code == 200);
    assert(strcmp(res.msg.body, "name: my_name") == 0);

    /* ---- cleanup ---- */
    http_client_close(&client);
    destroy_request(&req);
    destroy_response(&res);

    printf("Path with Query Param client-server test passed...\n");
    return 0;
}

int main()
{
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, server, NULL) != 0)
    {
        fprintf(stderr, "Error creating server thread\n");
        return 1;
    }
    client_root_req();
    client_param_req();
    printf("HTTP client and server integration tests passed.\n");
    return 0;
}