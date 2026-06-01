#include "app_middleware.h"

#include <blue-bird/log/log.h>

bb_error_t logger_middleware(bb_request_t *req, bb_response_t *res)
{
    BB_LOG_INFO("[Blue-Bird] %s %s; response status: %d\n", bb_request_get_method(req), bb_request_get_path(req), bb_response_get_status(res));
    return BB_SUCCESS();
}

bb_error_t server_header_middleware(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Server", "Blue-Bird/0.1");
    return BB_SUCCESS();
}
