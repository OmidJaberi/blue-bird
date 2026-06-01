#ifndef BB_REQUEST_H
#define BB_REQUEST_H

#include "server_request.h"
#include "client_request.h"

typedef enum {
    BB_SERVER_REQUEST,
    BB_CLIENT_REQUEST
} _bb_request_type_t;

typedef struct {
    _bb_request_type_t type;
    union {
        bb_server_request_t s_req;
        bb_client_request_t c_req;
    } inner_req;
} bb_request_t;

void bb_request_init_with_type(bb_request_t *req, _bb_request_type_t type);

void bb_request_init(bb_request_t *req); // For Client only

void bb_request_destroy(bb_request_t *req);

bb_http_message_t *bb_request_get_message(bb_request_t *req);

char *bb_request_get_method(bb_request_t *req);

// Server:

int bb_request_parse(const char *raw, bb_request_t *req);

int bb_request_add_param(bb_request_t *req, const char *key, const char *value);

const char *bb_request_get_param(bb_request_t *req, const char *name);

int bb_request_add_query_param(bb_request_t *req, const char *key, const char *value);

const char *bb_request_get_query_param(bb_request_t *req, const char *key);

const char *bb_request_get_header(bb_request_t *req, const char *name);

char *bb_request_get_path(bb_request_t *req);


// Client:

void bb_request_set_method(bb_request_t *req, const char *method);

void bb_request_set_url(bb_request_t *req, const char *url);

void bb_request_set_header(bb_request_t *req, const char *name, const char *value);

void bb_request_set_body(bb_request_t *req, char *body);

char *bb_request_get_url(bb_request_t *req);

// bb_request_get_params and bb_request_get_param_count ?

#endif //BB_REQUEST_H
