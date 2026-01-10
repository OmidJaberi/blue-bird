#include "core/server.h"

BBError root_handler(request_t *req, response_t *res)
{
    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Hello, Blue-Bird :)");
    return BB_SUCCESS();
}

int main()
{
    Server server;
    init_server(&server, 8080);

    add_route(&server, "GET", "/", root_handler);
    
    start_server(&server);
    return 0;
}
