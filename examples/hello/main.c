#include "app_handlers.h"
#include <stdio.h>
#include <blue-bird/runtime/runtime.h>
#include <blue-bird/web/server.h>

int main(void)
{
    bb_server_t *server = bb_server_create(8080);

    bb_server_add_route(server, "GET", "/", root_handler);
    bb_server_add_route(server, "POST", "/hello", hello_post_handler);
    bb_server_add_route(server, "GET", "/hello", hello_get_handler);
    
    bb_error_t start_err = bb_server_start(server);
    if (BB_FAILED(start_err))
    {
        fprintf(stderr, "Could not start the server: %s\n", start_err.msg);
        return 1;
    }
    bb_runtime_run_default();
    return 0;
}
