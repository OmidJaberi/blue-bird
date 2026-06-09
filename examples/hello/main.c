#include "app_handlers.h"
#include <blue-bird/runtime/runtime.h>
#include <blue-bird/web/server.h>

int main(void)
{
    bb_server_t *server = bb_server_create(8080);

    bb_server_add_route(server, "GET", "/", root_handler);
    bb_server_add_route(server, "POST", "/hello", hello_post_handler);
    bb_server_add_route(server, "GET", "/hello", hello_get_handler);
    
    bb_server_start(server);
    bb_runtime_run_default();
    return 0;
}
