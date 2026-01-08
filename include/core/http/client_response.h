#ifndef CLIENT_RESPONSE_H
#define CLIENT_RESPONSE_H

#include "message.h"

typedef struct {
    http_message_t msg;

    int status_code;
    char *status_text;
} client_response_t;

void init_client_response(client_response_t *res);
void destroy_client_response(client_response_t *res);
const char *get_client_header(client_response_t *res, const char *name);
int parse_client_response(const char *raw, client_response_t *res);

#endif //CLIENT_RESPONSE_H
