#include "core/router.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>


void handler_root(Request *req, Response *res)
{
    create_response(res, 200, "Root OK");
}

void handler_hello(Request *req, Response *res)
{
    create_response(res, 200, "Hello OK");
}

void test_route_match()
{
    add_route("/", handler_root);
    add_route("/hello", handler_hello);

    Request req;
    Response res;
    strcpy(req.path, "/hello");

    handle_request(&req, &res);
    assert(strcmp(res.body, "Hello OK") == 0);
}

void test_route_not_found()
{
    Request req;
    Response res;
    strcpy(req.path, "/doesnotexist");

    handle_request(&req, &res);
    assert(strcmp(res.body, "Not Found") == 0);
}

int main()
{
    test_route_match();
    test_route_not_found();
    printf("All Router tests passed.\n");
    return 0;
}
