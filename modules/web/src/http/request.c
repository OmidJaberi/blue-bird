#include "blue-bird/web/http/request.h"
#include "blue-bird/error/assert.h"

void bb_request_init_with_type(bb_request_t *req, _bb_request_type_t type)
{
    req->type = type;
    switch (type)
    {
    case BB_CLIENT_REQUEST:
        bb_client_request_init(&req->inner_req.c_req);
        break;
    case BB_SERVER_REQUEST:
        bb_server_request_init(&req->inner_req.s_req);
        break;
    }
}

void bb_request_init(bb_request_t *req) // For Client only
{
    bb_request_init_with_type(req, BB_CLIENT_REQUEST);
}

void bb_request_destroy(bb_request_t *req)
{
    switch (req->type)
    {
    case BB_CLIENT_REQUEST:
        bb_client_request_destroy(&req->inner_req.c_req);
        break;
    case BB_SERVER_REQUEST:
        bb_server_request_destroy(&req->inner_req.s_req);
        break;
    }
}

// Server:

int bb_request_parse(const char *raw, bb_request_t *req)
{
    BB_ASSERT(req->type == BB_SERVER_REQUEST, "Invalid request type.");
    return bb_server_request_parse(raw, &req->inner_req.s_req);
}

int bb_request_add_param(bb_request_t *req, const char *key, const char *value)
{
    BB_ASSERT(req->type == BB_SERVER_REQUEST, "Invalid request type.");
    return bb_server_request_add_param(&req->inner_req.s_req, key, value);
}

const char *bb_request_get_param(bb_request_t *req, const char *name)
{
    BB_ASSERT(req->type == BB_SERVER_REQUEST, "Invalid request type.");
    return bb_server_request_get_param(&req->inner_req.s_req, name);
}

int bb_request_add_query_param(bb_request_t *req, const char *key, const char *value)
{
    BB_ASSERT(req->type == BB_SERVER_REQUEST, "Invalid request type.");
    return bb_server_request_add_query_param(&req->inner_req.s_req, key, value);
}

const char *bb_request_get_query_param(bb_request_t *req, const char *key)
{
    BB_ASSERT(req->type == BB_SERVER_REQUEST, "Invalid request type.");
    return bb_server_request_get_query_param(&req->inner_req.s_req, key);
}

const char *bb_request_get_header(bb_request_t *req, const char *name)
{
    BB_ASSERT(req->type == BB_SERVER_REQUEST, "Invalid request type.");
    return bb_server_request_get_header(&req->inner_req.s_req, name);
}

// Client:

void bb_request_set_method(bb_request_t *req, const char *method)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    bb_client_request_set_method(&req->inner_req.c_req, method);
}

void bb_request_set_url(bb_request_t *req, const char *url)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    bb_client_request_set_url(&req->inner_req.c_req, url);
}

void bb_request_set_header(bb_request_t *req, const char *name, const char *value)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    bb_client_request_set_header(&req->inner_req.c_req, name, value);
}

void bb_request_set_body(bb_request_t *req, char *body)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    bb_client_request_set_body(&req->inner_req.c_req, body);
}
