#ifndef CLIENT_RESPONSE_H
#define CLIENT_RESPONSE_H

#include "server_response.h"

typedef server_response_t client_response_t;

void init_client_response(client_response_t *res);
void destroy_client_response(client_response_t *res);
const char *get_client_response_header(client_response_t *res, const char *name);
int parse_client_response(const char *raw, client_response_t *res);

#endif //CLIENT_RESPONSE_H
