#include "core/router.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// Handlers
BBError handler_root(request_t *req, response_t *res)
{
    init_response(res);
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Root OK");
    return BB_SUCCESS();
}

BBError handler_hello_get(request_t *req, response_t *res)
{
    init_response(res);
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Hello GET OK");
    return BB_SUCCESS();
}

BBError handler_hello_post(request_t *req, response_t *res)
{
    init_response(res);
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Hello POST OK");
    return BB_SUCCESS();
}

BBError handler_user(request_t *req, response_t *res)
{
    const char *id = get_request_param(req, "id");
    init_response(res);
    set_response_header(res, "Content-Type", "text/plain");
    
    char buf[64];
    snprintf(buf, sizeof(buf), "User ID: %s", id ? id : "none");
    set_response_body(res, buf);
    return BB_SUCCESS();
}

// Tests
void test_route_match_get()
{
    RouteList route_list;
    init_route_list(&route_list);

    request_t req;
    memset(&req, 0, sizeof(req));
    init_message(&GET_SERVER_REQUEST_MESSAGE(req));

    response_t res;
    memset(&res, 0, sizeof(res));
    init_response(&res);

    add_route_to_list(&route_list, "GET", "/", handler_root);
    add_route_to_list(&route_list, "GET", "/hello", handler_hello_get);

    strcpy(GET_REQUEST_METHOD(req), "GET");
    strcpy(GET_REQUEST_PATH(req), "/hello");

    handle_request(&route_list, &req, &res);
    assert(strcmp(res.msg.body, "Hello GET OK") == 0);
}

void test_route_match_post()
{
    RouteList route_list;
    init_route_list(&route_list);

    request_t req;
    memset(&req, 0, sizeof(req));
    init_message(&GET_SERVER_REQUEST_MESSAGE(req));

    response_t res;
    memset(&res, 0, sizeof(res));
    init_response(&res);

    add_route_to_list(&route_list, "POST", "/hello", handler_hello_post);

    strcpy(GET_REQUEST_METHOD(req), "POST");
    strcpy(GET_REQUEST_PATH(req), "/hello");

    handle_request(&route_list, &req, &res);
    assert(strcmp(res.msg.body, "Hello POST OK") == 0);
}

void test_route_not_found()
{
    RouteList route_list;
    init_route_list(&route_list);

    request_t req;
    memset(&req, 0, sizeof(req));
    init_message(&GET_SERVER_REQUEST_MESSAGE(req));

    response_t res;
    memset(&res, 0, sizeof(res));
    init_response(&res);

    strcpy(GET_REQUEST_METHOD(req), "GET");
    strcpy(GET_REQUEST_PATH(req), "/doesnotexist");

    handle_request(&route_list, &req, &res);
    assert(strcmp(res.msg.body, "Route Not Found") == 0);
}

void test_route_with_param()
{
    RouteList route_list;
    init_route_list(&route_list);

    request_t req;
    memset(&req, 0, sizeof(req));
    init_message(&GET_SERVER_REQUEST_MESSAGE(req));

    response_t res;
    memset(&res, 0, sizeof(res));
    init_response(&res);

    add_route_to_list(&route_list, "GET", "/users/:id", handler_user);

    strcpy(GET_REQUEST_METHOD(req), "GET");
    strcpy(GET_REQUEST_PATH(req), "/users/42");

    handle_request(&route_list, &req, &res);
    assert(strcmp(res.msg.body, "User ID: 42") == 0);
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
