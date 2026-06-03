#include "http/client_request.h"
#include "blue-bird/web/http/message.h"

#include <stdlib.h>
#include <string.h>

void bb_client_request_init(bb_client_request_t *req)
{
    if (!req) return;

    req->msg = bb_message_create();

    req->method = NULL;
    req->url = NULL;
    req->host = NULL;
    req->port = 80; // default HTTP port
}

void bb_client_request_set_method(bb_client_request_t *req, const char *method)
{
    if (!req) return;

    if (req->method) {
        free(req->method);
        req->method = NULL;
    }

    if (!method) return;

    req->method = strdup(method);
}

void bb_client_request_set_url(bb_client_request_t *req, const char *url)
{
    if (!req) return;

    if (req->url) {
        free(req->url);
        req->url = NULL;
    }

    if (!url) return;

    req->url = strdup(url);
}

void bb_client_request_destroy(bb_client_request_t *req)
{
    if (!req) return;

    bb_message_destroy(req->msg);

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
