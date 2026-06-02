#ifndef BB_REQUEST_H
#define BB_REQUEST_H

#include "message.h"

typedef struct bb_request bb_request_t;

bb_request_t *bb_request_client_create(void);

bb_request_t *bb_request_server_create(void);

static inline bb_request_t *bb_request_create(void) { return bb_request_client_create(); } // For Client only

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
