#include "app_middleware.h"
#include "log/log.h"

BBError logger_middleware(request_t *req, response_t *res)
{
    LOG_INFO("[Blue-Bird] %s %s\n", GET_REQUEST_METHOD(*req), GET_REQUEST_PATH(*req));
    return BB_SUCCESS();
}

BBError server_header_middleware(request_t *req, response_t *res)
{
    set_response_header(res, "Server", "Blue-Bird/0.1");
    return BB_SUCCESS();
}
