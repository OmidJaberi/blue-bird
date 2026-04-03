#include "core/http/client_request.h"
#include "core/http/message.h"

#include <stdlib.h>
#include <string.h>

void init_client_request(client_request_t *req)
{
    if (!req) return;

    init_message(&req->msg);

    req->method = NULL;
    req->url = NULL;
    req->host = NULL;
    req->port = 80; // default HTTP port
}

void set_client_request_method(client_request_t *req, const char *method)
{
    if (!req) return;

    if (req->method) {
        free(req->method);
        req->method = NULL;
    }

    if (!method) return;

    req->method = strdup(method);
}

void set_client_request_url(client_request_t *req, const char *url)
{
    if (!req) return;

    if (req->url) {
        free(req->url);
        req->url = NULL;
    }

    if (!url) return;

    req->url = strdup(url);
}

void set_client_request_header(client_request_t *req, const char *name, const char *value)
{
    if (!req) return;

    set_message_header(&req->msg, name, value);
}

void set_client_request_body(client_request_t *req, char *body)
{
    set_message_body(&req->msg, body);
}

void destroy_client_request(client_request_t *req)
{
    if (!req) return;

    destroy_message(&req->msg);

    if (req->method) {
        free(req->method);
        req->method = NULL;
    }

    if (req->url) {
        free(req->url);
        req->url = NULL;
    }

    if (req->host) {
        free(req->host);
        req->host = NULL;
    }
}
