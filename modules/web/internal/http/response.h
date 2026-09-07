#ifndef BB_RESPONSE_INTERNAL_H
#define BB_RESPONSE_INTERNAL_H

#include "http/http_parser.h"
#include "blue-bird/web/http/response.h"

int bb_response_from_parsed(const bb_http_response_t *parsed, bb_response_t *res);

#endif
