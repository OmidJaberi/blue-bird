#ifndef BB_SERVER_RESPONSE_H
#define BB_SERVER_RESPONSE_H

#include "message.h"

typedef struct {
    bb_http_message_t msg;

    int status_code;
    char *status_text;

} bb_server_response_t;

void bb_server_response_init(bb_server_response_t *res);

void bb_server_response_destroy(bb_server_response_t *res);

int bb_server_response_set_status(bb_server_response_t *res, int code);

void bb_server_response_set_header(bb_server_response_t *res, const char *name, const char *value);

void bb_server_response_set_body(bb_server_response_t *res, char *body);

int bb_server_response_serialize(bb_server_response_t *res, char **buffer, size_t *buffer_size);

#endif //BB_SERVER_RESPONSE_H
