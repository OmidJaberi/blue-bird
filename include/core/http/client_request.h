#ifndef CLIENT_REQUEST_H
#define CLIENT_REQUEST_H

#include <core/http/message.h>

typedef struct {
    http_message_t msg;

    char *method;
    char *url;
    char *host;
    int port;
} client_request_t;

#endif //CLIENT_REQUEST_H
