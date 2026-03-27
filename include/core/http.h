#ifndef BB_HTTP_H
#define BB_HTTP_H

#include "http/request.h"
#include "http/response.h"
#include "error/error.h"

#include <unistd.h>

typedef BBError (*http_handler_cb)(request_t *req, response_t *res);
ssize_t read_http_message(int fd, char **out_buf);

#endif //BB_HTTP_H
