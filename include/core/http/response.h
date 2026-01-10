#ifndef RESPONSE_H
#define RESPONSE_H

#include "server_response.h"
#include "client_response.h"

typedef server_response_t response_t;

void init_response(response_t *res);

void destroy_response(response_t *res);

// Server:

int set_response_status(response_t *res, int code);

void set_response_header(response_t *res, const char *name, const char *value);

void set_response_body(response_t *res, char *body);

int serialize_response(response_t *res, char *buffer, int buffer_size);

int send_response(int sock_fd, response_t *res);

// Client:

const char *get_response_header(response_t *res, const char *name);

int parse_response(const char *raw, response_t *res);

#endif // RESPONSE_H
