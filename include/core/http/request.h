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

//Helper Macros
#define GET_REQUEST_PATH(req) ((req).s_req.path)
#define GET_REQUEST_PARAMS(req) ((req).s_req.params)
#define GET_REQUEST_PARAM_COUNT(req) ((req).s_req.param_count)
#define GET_SERVER_REQUEST_MESSAGE(req) ((req).s_req.msg) // Works only for server right now...

#define GET_REQUEST_METHOD(req) ((req).c_req.method ? (req).c_req.method : (req).s_req.method)

#define GET_REQUEST_MESSAGE(req) ((req).c_req.msg) // Works only for client right now...
#define GET_REQUEST_URL(req) ((req).c_req.url)


#endif // REQUEST_H
