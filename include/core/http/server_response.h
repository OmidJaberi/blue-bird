#ifndef SERVER_RESPONSE_H
#define SERVER_RESPONSE_H

#include "message.h"

typedef struct {
    http_message_t msg;

    int status_code;
    char *status_text;

} server_response_t;

void init_server_response(server_response_t *res);

void destroy_server_response(server_response_t *res);

int set_server_response_status(server_response_t *res, int code);

void set_server_response_header(server_response_t *res, const char *name, const char *value);

void set_server_response_body(server_response_t *res, char *body);

int serialize_server_response(server_response_t *res, char *buffer, int buffer_size);

int send_server_response(int sock_fd, server_response_t *res);

#endif // SERVER_RESPONSE_H
