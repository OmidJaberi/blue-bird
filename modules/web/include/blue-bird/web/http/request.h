#ifndef BB_REQUEST_H
#define BB_REQUEST_H

#ifdef __cplusplus
extern "C" {
#endif


#include "message.h"

typedef struct bb_request bb_request_t;

bb_request_t *bb_request_client_create(void);

bb_request_t *bb_request_server_create(void);

void bb_request_destroy(bb_request_t *req);

void bb_request_reset(bb_request_t *req);

bb_http_message_t *bb_request_get_message(bb_request_t *req);

void bb_request_set_method(bb_request_t *req, const char *method);

char *bb_request_get_method(bb_request_t *req);

const char *bb_request_get_path(bb_request_t *req);

// Server:
void bb_request_set_path(bb_request_t *req, const char *path);

int bb_request_parse(const char *raw, bb_request_t *req);

int bb_request_add_param(bb_request_t *req, const char *key, const char *value);

const char *bb_request_get_param(bb_request_t *req, const char *name);

int bb_request_add_query_param(bb_request_t *req, const char *key, const char *value);

const char *bb_request_get_query_param(bb_request_t *req, const char *key);


// Client:

void bb_request_set_url(bb_request_t *req, const char *url);

const char *bb_request_get_url(bb_request_t *req);

const char *bb_request_get_scheme(bb_request_t *req);

const char *bb_request_get_host(bb_request_t *req);

int bb_request_get_port(bb_request_t *req);

int bb_request_serialize(bb_request_t *req, char **buffer, size_t *size);

// bb_request_get_params and bb_request_get_param_count ?

// Message Helpers:
static inline void bb_request_set_header(bb_request_t *req, const char *name, const char *value)
{
    bb_message_set_header(bb_request_get_message(req), name, value);
}

static inline const char *bb_request_get_header(bb_request_t *req, const char *name)
{
    return bb_message_get_header(bb_request_get_message(req), name);
}

static inline void bb_request_set_body(bb_request_t *req, char *body)
{
    bb_message_set_body(bb_request_get_message(req), body);
}

static inline const char *bb_request_get_body(bb_request_t *req)
{
    return bb_message_get_body(bb_request_get_message(req));
}


#ifdef __cplusplus
}
#endif

#endif //BB_REQUEST_H
