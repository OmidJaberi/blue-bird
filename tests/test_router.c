#include "core/router.h"
#include "core/http.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int called = 0;

void test_handler(Request *req, int client_fd)
{
    called++;
}

void test_router_basic()
{
    Request req;
    strcpy(req.method, "GET");
    strcpy(req.path, "/test");
    strcpy(req.version, "HTTP/1.1");

    add_route("/test", test_handler);

    called = 0;
    handle_request(&req, 0); // dummy client_fd
    assert(called == 1);
}

int main()
{
    test_router_basic();
    printf("Router tests passed.\n");
    return 0;
}
