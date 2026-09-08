#ifndef BB_REQUEST_INTERNAL_H
#define BB_REQUEST_INTERNAL_H

#include "blue-bird/web/http/request.h"
#include "http/parser.h"

int bb_request_parse_http_parser(const bb_http_request_t *parsed, bb_request_t *req);

#endif
