#include "core/client.h"
#include "core/http.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    HttpClient client;
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

    printf("HTTP client integration test passed.\n");
    return 0;
}
