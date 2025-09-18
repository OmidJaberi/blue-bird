#include "core/network.h"
#include "core/router.h"
#include "core/http.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>

void hello_get_handler(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello via GET!");
}

void hello_post_handler(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello via POST!");
}

void root_handler(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Blue-Bird :)");
}

void user_handler(Request *req, Response *res)
{
    const char *user_id = get_param(req, "id");
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    char msg[128];
    snprintf(msg, sizeof(msg), "User ID: %s", user_id ? user_id : "unknown");
    set_body(res, msg);
}

void comments_handler(Request *req, Response *res)
{
    const char *user_id = get_param(req, "id");
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    char msg[128];
    snprintf(msg, sizeof(msg), "Comments by user: %s", user_id ? user_id : "unknown");
    set_body(res, msg);
}

int main()
{
    add_route("GET", "/", root_handler);
    add_route("POST", "/hello", hello_post_handler);
    add_route("GET", "/hello", hello_get_handler);
    add_route("GET", "/users/:id", user_handler);
    add_route("GET", "/users/:id/comments", comments_handler);

    init_server(8080);
    start_server();
    return 0;
}
