#include "app_middleware.h"

#include <blue-bird/log/log.h>

bb_error_t logger_middleware(request_t *req, response_t *res)
{
    LOG_INFO("[Blue-Bird] %s %s; response status: %d\n", GET_REQUEST_METHOD(*req), GET_REQUEST_PATH(*req), res->status_code);
    return BB_SUCCESS();
}

bb_error_t server_header_middleware(request_t *req, response_t *res)
{
    (void) req;
    set_response_header(res, "Server", "Blue-Bird/0.1");
    return BB_SUCCESS();
}
