#ifndef REQUEST_H
#define REQUEST_H

#include "server_request.h"
#include "client_request.h"

typedef struct {
    server_request_t s_req;
    client_request_t c_req;
} request_t;

void init_request(request_t *req);

void destroy_request(request_t *req);

// Server:

int parse_request(const char *raw, request_t *req);

const char *get_request_param(request_t *req, const char *name);

int add_request_query_param(request_t *req, const char *key, const char *value);

const char *get_request_query_param(request_t *req, const char *key);

const char *get_request_header(request_t *req, const char *name);


// Client:

void set_request_method(request_t *req, const char *method);

void set_request_url(request_t *req, const char *url);

void set_request_header(request_t *req, const char *name, const char *value);

void set_request_body(request_t *req, const char *body, size_t len);

#endif // REQUEST_H
