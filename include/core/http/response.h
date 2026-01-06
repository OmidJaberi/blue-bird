#ifndef RESPONSE_H
#define RESPONSE_H

#include <core/http/message.h>

typedef struct {
    http_message_t msg;

    int status_code;
    char *status_text;

} response_t;

void init_response(response_t *res);

void destroy_response(response_t *res);

int set_status(response_t *res, int code);

void set_header(response_t *res, const char *name, const char *value);

void set_body(response_t *res, char *body);

int serialize_response(response_t *res, char *buffer, int buffer_size);

int send_response(int sock_fd, response_t *res);

#endif // RESPONSE_H
