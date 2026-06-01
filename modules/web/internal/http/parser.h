#ifndef BB_HTTP_PARSER_H
#define BB_HTTP_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

int bb_http_request_complete(const char *buffer, size_t length);


#ifdef __cplusplus
}
#endif

#endif
