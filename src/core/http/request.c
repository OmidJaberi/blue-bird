#include "core/http/request.h"
#include "error/assert.h"

void init_request_with_type(request_t *req, request_type type)
{
    req->type = type;
    switch (type)
    {
    case CLIENT_REQUEST:
        init_client_request(&req->inner_req.c_req);
        break;
    case SERVER_REQUEST:
        init_server_request(&req->inner_req.s_req);
        break;
    }
}

void init_request(request_t *req) // For Client only
{
    init_request_with_type(req, CLIENT_REQUEST);
}

void destroy_request(request_t *req)
{
    switch (req->type)
    {
    case CLIENT_REQUEST:
        destroy_client_request(&req->inner_req.c_req);
        break;
    case SERVER_REQUEST:
        destroy_server_request(&req->inner_req.s_req);
        break;
    }
}

// Server:

int parse_request(const char *raw, request_t *req)
{
    BB_ASSERT(req->type == SERVER_REQUEST, "Invalid request type.");
    return parse_server_request(raw, &req->inner_req.s_req);
}

const char *get_request_param(request_t *req, const char *name)
{
    BB_ASSERT(req->type == SERVER_REQUEST, "Invalid request type.");
    return get_server_request_param(&req->inner_req.s_req, name);
}

int add_request_query_param(request_t *req, const char *key, const char *value)
{
    BB_ASSERT(req->type == SERVER_REQUEST, "Invalid request type.");
    return add_server_request_query_param(&req->inner_req.s_req, key, value);
}

const char *get_request_query_param(request_t *req, const char *key)
{
    BB_ASSERT(req->type == SERVER_REQUEST, "Invalid request type.");
    return get_server_request_query_param(&req->inner_req.s_req, key);
}

const char *get_request_header(request_t *req, const char *name)
{
    BB_ASSERT(req->type == SERVER_REQUEST, "Invalid request type.");
    return get_server_request_header(&req->inner_req.s_req, name);
}

// Client:

void set_request_method(request_t *req, const char *method)
{
    BB_ASSERT(req->type == CLIENT_REQUEST, "Invalid request type.");
    set_client_request_method(&req->inner_req.c_req, method);
}

void set_request_url(request_t *req, const char *url)
{
    BB_ASSERT(req->type == CLIENT_REQUEST, "Invalid request type.");
    set_client_request_url(&req->inner_req.c_req, url);
}

void set_request_header(request_t *req, const char *name, const char *value)
{
    BB_ASSERT(req->type == CLIENT_REQUEST, "Invalid request type.");
    set_client_request_header(&req->inner_req.c_req, name, value);
}

void set_request_body(request_t *req, char *body)
{
    BB_ASSERT(req->type == CLIENT_REQUEST, "Invalid request type.");
    set_client_request_body(&req->inner_req.c_req, body);
}