#include "core/router.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// Handlers
int handler_root(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Root OK");
}

int handler_hello_get(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello GET OK");
}

int handler_hello_post(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello POST OK");
}

int handler_user(Request *req, Response *res)
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
    RouteList route_list;

    add_route_to_list(&route_list, "GET", "/", handler_root);
    add_route_to_list(&route_list, "GET", "/hello", handler_hello_get);

    Request req = {0};
    Response res;
    strcpy(req.method, "GET");
    strcpy(req.path, "/hello");

    handle_request(&route_list, &req, &res);
    assert(strcmp(res.body, "Hello GET OK") == 0);
}

void test_route_match_post()
{
    RouteList route_list;

    add_route_to_list(&route_list, "POST", "/hello", handler_hello_post);

    Request req = {0};
    Response res;
    strcpy(req.method, "POST");
    strcpy(req.path, "/hello");

    handle_request(&route_list, &req, &res);
    assert(strcmp(res.body, "Hello POST OK") == 0);
}

void test_route_not_found()
{
    RouteList route_list;

    Request req = {0};
    Response res;
    strcpy(req.method, "GET");
    strcpy(req.path, "/doesnotexist");

    handle_request(&route_list, &req, &res);
    assert(strcmp(res.body, "Route Not Found") == 0);
}

void test_route_with_param()
{
    RouteList route_list;

    add_route_to_list(&route_list, "GET", "/users/:id", handler_user);

    Request req = {0};
    Response res;
    strcpy(req.method, "GET");
    strcpy(req.path, "/users/42");

    handle_request(&route_list, &req, &res);
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
