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
    Server server;

    use_middleware(logger_middleware);
    use_middleware(server_header_middleware);

    init_server(&server, 8080);

    add_route(&server, "GET", "/", root_handler);
    add_route(&server, "POST", "/hello", hello_post_handler);
    add_route(&server, "GET", "/hello", hello_get_handler);
    add_route(&server, "GET", "/users/:id", user_handler);
    add_route(&server, "GET", "/users/:id/comments", comments_handler);
    
    start_server(&server);
    return 0;
}
