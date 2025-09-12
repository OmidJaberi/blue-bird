#include "core/network.h"
#include "core/router.h"
#include "core/http.h"
#include <unistd.h>
#include <string.h>

void hello_handler(Request *req, Response *res)
{
    create_response(res, 200, "Hello");
}

void root_handler(Request *req, Response *res)
{
    create_response(res, 200, "Blue-Bird :)");
}

int main()
{
    add_route("/", root_handler);
    add_route("/hello", hello_handler);

    init_server(8080);
    start_server();
    return 0;
}
