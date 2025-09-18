#include "core/router.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// Handlers
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

void handler_user(Request *req, Response *res)
{
    const char *id = get_param(req, "id");
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    
    char buf[64];
    snprintf(buf, sizeof(buf), "User ID: %s", id ? id : "none");
    set_body(res, buf);
}

// Tests
void test_route_match_get()
{
    clear_routes();

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
    clear_routes();

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
    clear_routes();

    Request req;
    Response res;
    strcpy(req.method, "GET");
    strcpy(req.path, "/doesnotexist");

    handle_request(&req, &res);
    assert(strcmp(res.body, "Route Not Found") == 0);
}

void test_route_with_param()
{
    clear_routes();

    add_route("GET", "/users/:id", handler_user);

    Request req;
    Response res;
    strcpy(req.method, "GET");
    strcpy(req.path, "/users/42");

    handle_request(&req, &res);
    assert(strcmp(res.body, "User ID: 42") == 0);
}

int main()
{
    test_route_match_get();
    test_route_match_post();
    test_route_not_found();
    test_route_with_param();
    printf("All Router tests passed.\n");
    return 0;
}
