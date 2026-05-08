#ifndef BB_REQUEST_H
#define BB_REQUEST_H

#include "server_request.h"
#include "client_request.h"

typedef enum {
    SERVER_REQUEST,
    CLIENT_REQUEST
} request_type;

typedef struct {
    request_type type;
    union {
        server_request_t s_req;
        client_request_t c_req;
    } inner_req;
} request_t;

void init_request_with_type(request_t *req, request_type type);

void init_request(request_t *req); // For Client only

void destroy_request(request_t *req);

static inline http_message_t *get_request_message(request_t *req)
{
    return (req->type == CLIENT_REQUEST)
        ? &req->inner_req.c_req.msg
        : &req->inner_req.s_req.msg;
}

// Server:

int parse_request(const char *raw, request_t *req);

int add_request_param(request_t *req, const char *key, const char *value);

const char *get_request_param(request_t *req, const char *name);

int add_request_query_param(request_t *req, const char *key, const char *value);

const char *get_request_query_param(request_t *req, const char *key);

const char *get_request_header(request_t *req, const char *name);


// Client:

void set_request_method(request_t *req, const char *method);

void set_request_url(request_t *req, const char *url);

void set_request_header(request_t *req, const char *name, const char *value);

void set_request_body(request_t *req, char *body);

//Helper Macros
#define GET_REQUEST_PATH(req) ((req).inner_req.s_req.path)
#define GET_REQUEST_PARAMS(req) ((req).inner_req.s_req.params)
#define GET_REQUEST_PARAM_COUNT(req) ((req).inner_req.s_req.param_count)

#define GET_REQUEST_METHOD(req) ((req).type == CLIENT_REQUEST ? (req).inner_req.c_req.method : (req).inner_req.s_req.method)
#define GET_REQUEST_MESSAGE(req) (*(get_request_message(&(req))))

#define GET_REQUEST_URL(req) ((req).inner_req.c_req.url)


#endif //BB_REQUEST_H
