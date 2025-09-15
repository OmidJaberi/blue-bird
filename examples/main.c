#include "core/network.h"
#include "core/router.h"
#include "core/http.h"
#include <unistd.h>
#include <string.h>

void hello_handler(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello");
}

void root_handler(Request *req, Response *res)
{
    init_response(res);
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Blue-Bird :)");
}

int main()
{
    add_route("/", root_handler);
    add_route("/hello", hello_handler);

    init_server(8080);
    start_server();
    return 0;
}
