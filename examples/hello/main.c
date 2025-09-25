#include "app_middleware.h"
#include "app_handlers.h"

#include "core/network.h"
#include "core/router.h"
#include "core/http.h"
#include "core/middleware.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main()
{
    use_middleware(logger_middleware);
    use_middleware(server_header_middleware);

    add_route("GET", "/", root_handler);
    add_route("POST", "/hello", hello_post_handler);
    add_route("GET", "/hello", hello_get_handler);
    add_route("GET", "/users/:id", user_handler);
    add_route("GET", "/users/:id/comments", comments_handler);

    init_server(8080);
    start_server();
    return 0;
}
