#include "core/client.h"
#include "core/http/client_request.h"
#include "core/http/client_response.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    HttpClient client;
    client_request_t req;
    client_response_t res;

    /* ---- init ---- */
    init_client_request(&req);
    init_client_response(&res);

    /* ---- build request ---- */
    set_client_method(&req, "GET");
    set_client_url(&req, "/");
    set_client_header(&req, "Host", "127.0.0.1");
    set_client_header(&req, "Connection", "close");

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
    destroy_client_request(&req);
    destroy_client_response(&res);

    printf("HTTP client integration test passed.\n");
    return 0;
}
