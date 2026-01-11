#include "core/http/request.h"


void init_request(request_t *req)
{
    init_server_request(&req->s_req);
    init_client_request(&req->c_req);
}

void destroy_request(request_t *req)
{
    destroy_client_request(&req->c_req);
    destroy_server_request(&req->s_req);
}

// Server:

int parse_request(const char *raw, request_t *req)
{
    return parse_server_request(raw, &req->s_req);
}

const char *get_request_param(request_t *req, const char *name)
{
    return get_server_request_param(&req->s_req, name);
}

int add_request_query_param(request_t *req, const char *key, const char *value)
{
    return add_server_request_query_param(&req->s_req, key, value);
}

const char *get_request_query_param(request_t *req, const char *key)
{
    return get_server_request_query_param(&req->s_req, key);
}

const char *get_request_header(request_t *req, const char *name)
{
    return get_server_request_header(&req->s_req, name);
}


// Client:

void set_request_method(request_t *req, const char *method)
{
    set_client_request_method(&req->c_req, method);
}

void set_request_url(request_t *req, const char *url)
{
    set_client_request_url(&req->c_req, url);
}

void set_request_header(request_t *req, const char *name, const char *value)
{
    set_client_request_header(&req->c_req, name, value);
}

void set_request_body(request_t *req, const char *body, size_t len)
{
    set_client_request_body(&req->c_req, body, len);
}