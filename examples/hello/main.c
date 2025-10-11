#include "app_middleware.h"
#include "app_handlers.h"

#include "core/server.h"
#include "core/router.h"
#include "core/http.h"
#include "core/middleware.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main()
{
    Server server;
    init_server(&server, 8080);

    use_middleware(&server, logger_middleware);
    use_middleware(&server, server_header_middleware);

    add_route(&server, "GET", "/", root_handler);
    add_route(&server, "POST", "/hello", hello_post_handler);
    add_route(&server, "GET", "/hello", hello_get_handler);
    add_route(&server, "GET", "/users/:id", user_handler);
    add_route(&server, "GET", "/users/:id/comments", comments_handler);
    
    start_server(&server);
    return 0;
}
