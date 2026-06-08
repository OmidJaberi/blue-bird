#include "blue-bird/web/http/request.h"
#include "blue-bird/error/assert.h"

#include "http/server_request.h"
#include "http/client_request.h"

typedef enum {
    BB_SERVER_REQUEST,
    BB_CLIENT_REQUEST
} _bb_request_type_t;

struct bb_request {
    _bb_request_type_t type;
    union {
        bb_server_request_t s_req;
        bb_client_request_t c_req;
    } inner_req;
};

static bb_request_t *_bb_request_create_with_type(_bb_request_type_t type)
{
    bb_request_t *req = malloc(sizeof(bb_request_t));
    if (!req)
    {
        return NULL;
    }
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
    return req;
}

bb_request_t *bb_request_client_create(void)
{
    return _bb_request_create_with_type(BB_CLIENT_REQUEST);
}

bb_request_t *bb_request_server_create(void)
{
    return _bb_request_create_with_type(BB_SERVER_REQUEST);
}

void bb_request_destroy(bb_request_t *req)
{
    if (!req)
    {
        return;
    }
    switch (req->type)
    {
    case BB_CLIENT_REQUEST:
        bb_client_request_destroy(&req->inner_req.c_req);
        break;
    case BB_SERVER_REQUEST:
        bb_server_request_destroy(&req->inner_req.s_req);
        break;
    }
    free(req);
}

void bb_request_reset(bb_request_t *req)
{

    if (!req)
    {
        return;
    }
    switch (req->type)
    {
    case BB_CLIENT_REQUEST:
        bb_client_request_reset(&req->inner_req.c_req);
        break;
    case BB_SERVER_REQUEST:
        bb_server_request_reset(&req->inner_req.s_req);
        break;
    }
}

bb_http_message_t *bb_request_get_message(bb_request_t *req)
{
    return (req->type == BB_CLIENT_REQUEST)
        ? req->inner_req.c_req.msg
        : req->inner_req.s_req.msg;
}

char *bb_request_get_method(bb_request_t *req)
{
    return req->type == BB_CLIENT_REQUEST ? req->inner_req.c_req.method : req->inner_req.s_req.method;
}

void bb_request_set_method(bb_request_t *req, const char *method)
{
    if (req->type == BB_CLIENT_REQUEST)
        bb_client_request_set_method(&req->inner_req.c_req, method);
    else
        bb_server_request_set_method(&req->inner_req.s_req, method);
}

const char *bb_request_get_path(bb_request_t *req)
{
    return req->type == BB_SERVER_REQUEST ? req->inner_req.s_req.path : bb_client_request_get_path(&req->inner_req.c_req);
}

// Server:

void bb_request_set_path(bb_request_t *req, const char *path)
{
    BB_ASSERT(req->type == BB_SERVER_REQUEST, "Invalid request type.");
    bb_server_request_set_path(&req->inner_req.s_req, path);
}

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

// Client:

void bb_request_set_url(bb_request_t *req, const char *url)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    bb_client_request_set_url(&req->inner_req.c_req, url);
}

const char *bb_request_get_url(bb_request_t *req)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    return req->inner_req.c_req.url;
}

const char *bb_request_get_scheme(bb_request_t *req)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    return bb_client_request_get_scheme(&req->inner_req.c_req);
}

const char *bb_request_get_host(bb_request_t *req)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    return bb_client_request_get_host(&req->inner_req.c_req);
}

int bb_request_get_port(bb_request_t *req)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    return bb_client_request_get_port(&req->inner_req.c_req);
}

int bb_request_serialize(bb_request_t *req, char **buffer, size_t *size)
{
    BB_ASSERT(req->type == BB_CLIENT_REQUEST, "Invalid request type.");
    const char *method = bb_request_get_method(req) ? bb_request_get_method(req) : "GET";
    const char *path = bb_request_get_path(req) ? bb_request_get_path(req) : "/";
    
    char start_line[512];
    snprintf(start_line, sizeof(start_line), "%s %s HTTP/1.1", method, path);

    // Temporary:
    bb_message_set_start_line(bb_request_get_message(req), start_line);
    return bb_message_serialize(req->inner_req.c_req.msg, buffer, size);
}
