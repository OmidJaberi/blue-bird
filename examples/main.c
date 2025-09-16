#include "core/network.h"
#include "core/router.h"
#include "core/http.h"
#include <unistd.h>
#include <string.h>

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

int main()
{
    add_route("GET", "/", root_handler);
    add_route("POST", "/hello", hello_post_handler);
    add_route("GET", "/hello", hello_get_handler);

    init_server(8080);
    start_server();
    return 0;
}
