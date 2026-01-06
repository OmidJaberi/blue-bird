#ifndef CLIENT_REQUEST_H
#define CLIENT_REQUEST_H

#include <stddef.h>
#include "message.h"

typedef struct {
    http_message_t msg;

    char *method;
    char *url;
    char *host;
    int port;
} client_request_t;

void init_client_request(client_request_t *req);
void set_client_method(client_request_t *req, const char *method);
void set_client_url(client_request_t *req, const char *url);
void set_client_header(client_request_t *req, const char *name, const char *value);
void set_client_body(client_request_t *req, const char *body, size_t len);
void destroy_client_request(client_request_t *req);

#endif //CLIENT_REQUEST_H
