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

void set_client_method(client_request_t *req, const char *method)
{
    if (!req) return;

    if (req->method) {
        free(req->method);
        req->method = NULL;
    }

    if (!method) return;

    req->method = strdup(method);
}

void set_client_url(client_request_t *req, const char *url)
{
    if (!req) return;

    if (req->url) {
        free(req->url);
        req->url = NULL;
    }

    if (!url) return;

    req->url = strdup(url);
}

void set_client_header(client_request_t *req, const char *name, const char *value)
{
    if (!req) return;

    set_message_header(&req->msg, name, value);
}

void set_client_body(client_request_t *req, const char *body, size_t len)
{
    if (!req) return;

    /* set_message_body() currently copies via strlen(),
       so we must respect len explicitly */
    if (req->msg.body) {
        free(req->msg.body);
        req->msg.body = NULL;
        req->msg.body_len = 0;
    }

    if (!body || len == 0) return;

    req->msg.body = malloc(len + 1);
    if (!req->msg.body) return;

    memcpy(req->msg.body, body, len);
    req->msg.body[len] = '\0';
    req->msg.body_len = len;
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
