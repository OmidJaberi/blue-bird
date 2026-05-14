#ifndef BB_CLIENT_RESPONSE_H
#define BB_CLIENT_RESPONSE_H

#include "server_response.h"

typedef bb_server_response_t bb_client_response_t;

void bb_client_response_init(bb_client_response_t *res);
void bb_client_response_destroy(bb_client_response_t *res);
const char *bb_client_response_get_header(bb_client_response_t *res, const char *name);
int bb_client_response_parse(const char *raw, bb_client_response_t *res);

#endif //BB_CLIENT_RESPONSE_H
