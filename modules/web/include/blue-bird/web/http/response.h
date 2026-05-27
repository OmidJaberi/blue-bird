#ifndef BB_RESPONSE_H
#define BB_RESPONSE_H

#include "server_response.h"
#include "client_response.h"

typedef bb_server_response_t bb_response_t;

void bb_response_init(bb_response_t *res);

void bb_response_destroy(bb_response_t *res);

// Server:

int bb_response_set_status(bb_response_t *res, int code);

void bb_response_set_header(bb_response_t *res, const char *name, const char *value);

void bb_response_set_body(bb_response_t *res, char *body);

int bb_response_serialize(bb_response_t *res, char **buffer, size_t *size);

// Client:

const char *bb_response_get_header(bb_response_t *res, const char *name);

int bb_response_parse(const char *raw, bb_response_t *res);

#endif //BB_RESPONSE_H
