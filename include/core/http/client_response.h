#ifndef CLIENT_RESPONSE_H
#define CLIENT_RESPONSE_H

#include <core/http/message.h>

typedef struct {
    http_message_t msg;

    int status_code;
    char *status_text;
} client_response_h;

#endif //CLIENT_RESPONSE_H
