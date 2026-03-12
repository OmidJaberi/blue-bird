#include "core/server.h"
#include "core/client.h"
#include "core/http.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

char *response_message = "Hello, Blue-Bird :)";

BBError root_handler(request_t *req, response_t *res)
{
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, response_message);
    return BB_SUCCESS();
}

void *server(void* arg)
{
    bb_server_t server;
    init_server(&server, 8080);

    add_route(&server, "GET", "/", root_handler);
    
    start_server(&server);
}

int client()
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
    assert(strcmp(res.msg.body, response_message) == 0);

    /* ---- cleanup ---- */
    http_client_close(&client);
    destroy_request(&req);
    destroy_response(&res);

    printf("HTTP client and server integration test passed.\n");
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
    client();
    // pthread_join(thread_id, NULL);
    return 0;
}