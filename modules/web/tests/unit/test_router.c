#include "router.h"
#include <blue-bird/error/assert.h>
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

static bb_error_t websocket_handler(bb_websocket_t *ws, const bb_ws_message_t *message)
{
    (void) ws;
    (void) message;

    return BB_SUCCESS();
}

//handle_request
static void _handle_request(bb_route_list_t *route_list, bb_request_t *req, bb_response_t *res)
{
    bb_route_t *match = bb_route_list_match(route_list, req);

    if (match)
    {
        if (bb_route_get_type(match) == BB_ROUTE_HTTP)
        {
            bb_route_get_http_handler(match)(req, res);
        }
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
    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    bb_route_list_add_http(route_list, "GET", "/", handler_root);
    bb_route_list_add_http(route_list, "GET", "/hello", handler_hello_get);

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, "/hello");

    _handle_request(route_list, req, res);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Hello GET OK") == 0);
    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
    bb_response_destroy(res);
}

void test_route_match_post(void)
{
    printf("Testing Router: match POST...\n");
    bb_route_list_t *route_list = bb_route_list_create();
    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    bb_route_list_add_http(route_list, "POST", "/hello", handler_hello_post);

    bb_request_set_method(req, "POST");
    bb_request_set_path(req, "/hello");

    _handle_request(route_list, req, res);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Hello POST OK") == 0);
    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
    bb_response_destroy(res);
}

void test_route_not_found(void)
{
    printf("Testing Router: route not found...\n");
    bb_route_list_t *route_list = bb_route_list_create();
    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, "/doesnotexist");

    _handle_request(route_list, req, res);
    BB_ASSERT(strcmp(bb_response_get_body(res), "Route Not Found") == 0);
    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
    bb_response_destroy(res);
}

void test_route_with_param(void)
{
    printf("Testing Router: route with param...\n");
    bb_route_list_t *route_list = bb_route_list_create();
    bb_request_t *req = bb_request_server_create();
    bb_response_t *res = bb_response_create();

    bb_route_list_add_http(route_list, "GET", "/users/:id", handler_user);

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, "/users/42");

    _handle_request(route_list, req, res);
    BB_ASSERT(strcmp(bb_response_get_body(res), "User ID: 42") == 0);
    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
    bb_response_destroy(res);
}

void test_http_route_type(void)
{
    printf("Testing Router: HTTP route type...\n");

    bb_route_list_t *route_list = bb_route_list_create();

    bb_request_t *req = bb_request_server_create();

    bb_route_list_add_http(route_list, "GET", "/hello", handler_hello_get);

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, "/hello");

    bb_route_t *route = bb_route_list_match(route_list, req);

    BB_ASSERT(route != NULL);

    BB_ASSERT(bb_route_get_type(route) == BB_ROUTE_HTTP);

    BB_ASSERT(bb_route_get_http_handler(route) == handler_hello_get);

    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
}

void test_websocket_route_type(void)
{
    printf("Testing Router: WebSocket route type...\n");

    bb_route_list_t *route_list = bb_route_list_create();

    bb_request_t *req = bb_request_server_create();

    bb_route_list_add_websocket(route_list, "/chat", websocket_handler);

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, "/chat");

    bb_route_t *route = bb_route_list_match(route_list, req);

    BB_ASSERT(route != NULL);

    BB_ASSERT(bb_route_get_type(route) == BB_ROUTE_WEBSOCKET);

    BB_ASSERT(bb_route_get_websocket_handler(route) == websocket_handler);

    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
}

void test_websocket_route_only_matches_get(void)
{
    printf("Testing Router: websocket GET only...\n");

    bb_route_list_t *route_list = bb_route_list_create();

    bb_request_t *req = bb_request_server_create();

    bb_route_list_add_websocket(route_list, "/chat", websocket_handler);

    bb_request_set_method(req, "POST");
    bb_request_set_path(req, "/chat");

    bb_route_t *route = bb_route_list_match(route_list, req);

    BB_ASSERT(route == NULL);

    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
}

void test_long_request_segment(void)
{
    printf("Testing Router: long request segment...\n");
    bb_route_list_t *route_list = bb_route_list_create();
    bb_request_t *req = bb_request_server_create();

    bb_route_list_add_http(route_list, "GET", "/users/:id", handler_user);

    /* The request path buffer holds 255 bytes, so this is the longest
     * segment a request can carry: it must match without overflowing. */
    char path[300];
    memcpy(path, "/users/", 7);
    memset(path + 7, 'A', 248);
    path[255] = '\0';

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, path);
    BB_ASSERT(bb_route_list_match(route_list, req) != NULL);

    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
}

void test_too_many_request_segments_rejected(void)
{
    printf("Testing Router: too many request segments are rejected...\n");
    bb_route_list_t *route_list = bb_route_list_create();
    bb_request_t *req = bb_request_server_create();

    /* Route with exactly 20 segments */
    char route_path[128] = "";
    for (int i = 0; i < 20; i++)
        strcat(route_path, "/a");
    BB_ASSERT(bb_route_list_add_http(route_list, "GET", route_path, handler_hello_get).code == BB_OK);

    /* Request with 25 segments must not match by being truncated to 20 */
    char req_path[128] = "";
    for (int i = 0; i < 25; i++)
        strcat(req_path, "/a");

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, req_path);
    BB_ASSERT(bb_route_list_match(route_list, req) == NULL);

    bb_request_set_path(req, route_path);
    BB_ASSERT(bb_route_list_match(route_list, req) != NULL);

    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
}

void test_invalid_route_registration_rejected(void)
{
    printf("Testing Router: invalid route registration is rejected...\n");
    bb_route_list_t *route_list = bb_route_list_create();
    bb_request_t *req = bb_request_server_create();

    char path[512];
    path[0] = '/';
    memset(path + 1, 'B', 400);
    path[401] = '\0';

    BB_ASSERT(bb_route_list_add_http(route_list, "GET", path, handler_hello_get).code != BB_OK);
    BB_ASSERT(bb_route_list_add_websocket(route_list, path, websocket_handler).code != BB_OK);

    bb_request_set_method(req, "GET");
    bb_request_set_path(req, path);
    BB_ASSERT(bb_route_list_match(route_list, req) == NULL);

    bb_route_list_destroy(route_list);
    bb_request_destroy(req);
}

int main(void)
{
    test_route_match_get();
    test_route_match_post();
    test_route_not_found();
    test_route_with_param();
    test_http_route_type();
    test_websocket_route_type();
    test_websocket_route_only_matches_get();
    test_long_request_segment();
    test_too_many_request_segments_rejected();
    test_invalid_route_registration_rejected();
    printf("All Router tests passed.\n");
    return 0;
}
