#include "core/router.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>


void handler_root(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Root OK");
}

void handler_hello_get(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello GET OK");
}

void handler_hello_post(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello POST OK");
}

void test_route_match_get()
{
    add_route("GET", "/", handler_root);
    add_route("GET", "/hello", handler_hello_get);

    Request req;
    Response res;
    strcpy(req.method, "GET");
    strcpy(req.path, "/hello");

    handle_request(&req, &res);
    assert(strcmp(res.body, "Hello GET OK") == 0);
}

void test_route_match_post()
{
    add_route("POST", "/hello", handler_hello_post);

    Request req;
    Response res;
    strcpy(req.method, "POST");
    strcpy(req.path, "/hello");

    handle_request(&req, &res);
    assert(strcmp(res.body, "Hello POST OK") == 0);
}

void test_route_not_found()
{
    Request req;
    Response res;
    strcpy(req.method, "GET");
    strcpy(req.path, "/doesnotexist");

    handle_request(&req, &res);
    assert(strcmp(res.body, "Route Not Found") == 0);
}

int main()
{
    test_route_match_get();
    test_route_match_post();
    test_route_not_found();
    printf("All Router tests passed.\n");
    return 0;
}
