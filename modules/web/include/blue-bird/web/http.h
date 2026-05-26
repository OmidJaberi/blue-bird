#ifndef BB_HTTP_H
#define BB_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif


#include "http/request.h"
#include "http/response.h"
#include "blue-bird/error/error.h"

#include <unistd.h>

typedef bb_error_t (*bb_http_handler_cb)(bb_request_t *req, bb_response_t *res);

#ifdef __cplusplus
}
#endif

#endif //BB_HTTP_H
