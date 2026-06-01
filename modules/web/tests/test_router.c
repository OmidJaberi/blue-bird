#include "router.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

// Handlers
bb_error_t handler_root(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Root OK");
    return BB_SUCCESS();
}

bb_error_t handler_hello_get(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello GET OK");
    return BB_SUCCESS();
}

bb_error_t handler_hello_post(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello POST OK");
    return BB_SUCCESS();
}

bb_error_t handler_user(bb_request_t *req, bb_response_t *res)
{
    const char *id = bb_request_get_param(req, "id");
    bb_response_set_header(res, "Content-Type", "text/plain");
    
    char buf[64];
    snprintf(buf, sizeof(buf), "User ID: %s", id ? id : "none");
    bb_response_set_body(res, buf);
    return BB_SUCCESS();
}

//handle_request
static void _handle_request(bb_route_list_t *route_list, bb_request_t *req, bb_response_t *res)
{
    bb_route_t *match = bb_route_list_match(route_list, req);

    if (match)
    {
        bb_route_get_handler(match)(req, res);
    }
    else
    {
        // Default 404
        bb_response_set_status(res, 404);
        bb_response_set_header(res, "Content-Type", "text/plain");
        bb_response_set_body(res, "Route Not Found");
    }
}

// Tests
void test_route_match_get(void)
{
    printf("Testing Router: match GET...\n");
    bb_route_list_t *route_list = bb_route_list_create();

    bb_request_t req;
    memset(&req, 0, sizeof(req));
    bb_message_init(bb_request_get_message(&req));

    bb_response_t *res = bb_response_create();

    bb_route_list_add(route_list, "GET", "/", handler_root);
    bb_route_list_add(route_list, "GET", "/hello", handler_hello_get);

    strcpy(bb_request_get_method(&req), "GET");
    strcpy(bb_request_get_path(&req), "/hello");

    _handle_request(route_list, &req, res);
    assert(strcmp(bb_response_get_message(res)->body, "Hello GET OK") == 0);
}

void test_route_match_post(void)
{
    printf("Testing Router: match POST...\n");
    bb_route_list_t *route_list = bb_route_list_create();

    bb_request_t req;
    memset(&req, 0, sizeof(req));
    bb_message_init(bb_request_get_message(&req));

    bb_response_t *res = bb_response_create();

    bb_route_list_add(route_list, "POST", "/hello", handler_hello_post);

    strcpy(bb_request_get_method(&req), "POST");
    strcpy(bb_request_get_path(&req), "/hello");

    _handle_request(route_list, &req, res);
    assert(strcmp(bb_response_get_message(res)->body, "Hello POST OK") == 0);
}

void test_route_not_found(void)
{
    printf("Testing Router: route not found...\n");
    bb_route_list_t *route_list = bb_route_list_create();

    bb_request_t req;
    memset(&req, 0, sizeof(req));
    bb_message_init(bb_request_get_message(&req));

    bb_response_t *res = bb_response_create();

    strcpy(bb_request_get_method(&req), "GET");
    strcpy(bb_request_get_path(&req), "/doesnotexist");

    _handle_request(route_list, &req, res);
    assert(strcmp(bb_response_get_message(res)->body, "Route Not Found") == 0);
}

void test_route_with_param(void)
{
    printf("Testing Router: route with param...\n");
    bb_route_list_t *route_list = bb_route_list_create();

    bb_request_t req;
    memset(&req, 0, sizeof(req));
    bb_message_init(bb_request_get_message(&req));

    bb_response_t *res = bb_response_create();

    bb_route_list_add(route_list, "GET", "/users/:id", handler_user);

    strcpy(bb_request_get_method(&req), "GET");
    strcpy(bb_request_get_path(&req), "/users/42");

    _handle_request(route_list, &req, res);
    assert(strcmp(bb_response_get_message(res)->body, "User ID: 42") == 0);
}

int main(void)
{
    test_route_match_get();
    test_route_match_post();
    test_route_not_found();
    test_route_with_param();
    printf("All Router tests passed.\n");
    return 0;
}
