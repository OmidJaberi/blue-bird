#include "http/client_request.h"
#include "blue-bird/web/http/message.h"

#include <stdlib.h>
#include <string.h>

static void _bb_replace_string(char **dst, const char *src)
{
    if (*dst)
    {
        free(*dst);
        *dst = NULL;
    }

    if (src)
    {
        *dst = strdup(src);
    }
}

void bb_client_request_init(bb_client_request_t *req)
{
    if (!req)
    {
        return;
    }

    req->msg = bb_message_create();

    req->method = NULL;
    req->url = NULL;

    req->scheme = NULL;
    req->host = NULL;
    req->port = -1;
    req->path = NULL;
}

void bb_client_request_destroy(bb_client_request_t *req)
{
    if (!req)
    {
        return;
    }

    bb_message_destroy(req->msg);

    free(req->method);
    free(req->url);

    free(req->scheme);
    free(req->host);
    free(req->path);

    req->method = NULL;
    req->url = NULL;

    req->scheme = NULL;
    req->host = NULL;
    req->path = NULL;
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
    if (!req)
    {
        return;
    }

    _bb_replace_string(&req->url, url);

    free(req->scheme);
    free(req->host);
    free(req->path);

    req->scheme = NULL;
    req->host = NULL;
    req->path = NULL;
    req->port = 80;

    if (!url)
    {
        return;
    }

    /*
     * Relative URL.
     *
     * Example:
     *   /users?id=1
     */
    if (url[0] == '/')
    {
        req->path = strdup(url);
        return;
    }

    const char *scheme_end = strstr(url, "://");

    if (!scheme_end)
    {
        req->path = strdup(url);
        return;
    }

    /*
     * Scheme
     */
    {
        size_t len = (size_t)(scheme_end - url);

        req->scheme = malloc(len + 1);

        memcpy(req->scheme, url, len);
        req->scheme[len] = '\0';
    }

    if (strcmp(req->scheme, "https") == 0)
    {
        req->port = 443;
    }

    const char *authority = scheme_end + 3;

    const char *path_start = strchr(authority, '/');

    /*
     * Host only:
     * http://example.com
     */
    if (!path_start)
    {
        path_start = authority + strlen(authority);

        req->path = strdup("/");
    }
    else
    {
        req->path = strdup(path_start);
    }

    /*
     * Host[:port]
     */
    const char *host_end = path_start;

    const char *colon = NULL;

    for (const char *p = authority; p < host_end; p++)
    {
        if (*p == ':')
        {
            colon = p;
            break;
        }
    }

    if (colon)
    {
        size_t host_len = (size_t)(colon - authority);

        req->host = malloc(host_len + 1);

        memcpy(req->host, authority, host_len);
        req->host[host_len] = '\0';

        req->port = atoi(colon + 1);
    }
    else
    {
        size_t host_len = (size_t)(host_end - authority);

        req->host = malloc(host_len + 1);

        memcpy(req->host, authority, host_len);
        req->host[host_len] = '\0';
    }
}

const char *bb_client_request_get_url(bb_client_request_t *req)
{
    return req ? req->url : NULL;
}

const char *bb_client_request_get_scheme(bb_client_request_t *req)
{
    return req ? req->scheme : NULL;
}

const char *bb_client_request_get_host(bb_client_request_t *req)
{
    return req ? req->host : NULL;
}

int bb_client_request_get_port(bb_client_request_t *req)
{
    return req ? req->port : -1;
}

const char *bb_client_request_get_path(bb_client_request_t *req)
{
    return req ? req->path : NULL;
}
