#include "app_handlers.h"
#include "core/server.h"

int main()
{
    bb_server_t server;
    init_server(&server, 8080);

    add_route(&server, "GET", "/", root_handler);
    add_route(&server, "POST", "/hello", hello_post_handler);
    add_route(&server, "GET", "/hello", hello_get_handler);
    
    start_server(&server);
    return 0;
}
