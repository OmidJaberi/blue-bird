#include "http/server_request.h"
#include "blue-bird/utils/encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_valid_path(const char *path)
{
    for (const unsigned char *p = (const unsigned char *)path; *p; p++)
    {
        unsigned char c = *p;

        // Reject control chars
        if (c < 32 || c == 127)
            return 0;

        // Reject unsafe / problematic characters
        switch (c)
        {
            case ' ':
            case '"':
            case '<':
            case '>':
            case '\\':
            case '^':
            case '`':
            case '{':
            case '|':
            case '}':
                return 0;
        }
    }

    return 1;
}

void bb_server_request_init(bb_server_request_t *req)
{
    if (!req) return;
    req->msg = bb_message_create();
    req->param_count = 0;
    req->query_count = 0;
}

void bb_server_request_destroy(bb_server_request_t *req)
{
    if (!req) return;
    if (req->msg)
    {
        bb_message_destroy(req->msg);
    }
    req->param_count = 0;
    req->query_count = 0;
}

void bb_server_request_reset(bb_server_request_t *req)
{
    if (!req) return;
    if (req->msg)
    {
        bb_message_reset(req->msg);
    }
    req->param_count = 0;
    req->query_count = 0;
}

static void parse_query_params(bb_server_request_t *req)
{
    char *qmark = strchr(req->path, '?');
    if (qmark)
    {
        *qmark = '\0';
        char *query_str = qmark + 1;
        char *pair = strtok(query_str, "&");
        while (pair)
        {
            char *eq = strchr(pair, '=');

            bb_decode_percent(pair, 1);      // '+' becomes space in query
            
            if (eq)
            {
                bb_decode_percent(eq + 1, 1);
                *eq = '\0';
                bb_server_request_add_query_param(req, pair, eq + 1);

            }
            else
                bb_server_request_add_query_param(req, pair, "");
            pair = strtok(NULL, "&");
        }
    }
}

int bb_server_request_parse(const char *raw, bb_server_request_t *req)
{
    if (!raw || !req) return -1;

    req->param_count = 0;
    req->query_count = 0;
    if (bb_message_parse(raw, req->msg) != 0)
        return -1;

    // Parse method, path, version
    char method[8];
    char path[4096];
    char version[16];

    if (sscanf(bb_message_get_start_line(req->msg), "%7s %4095s %15s", method, path, version) != 3)
        return -1;

    if (strlen(path) >= 256)
        return -1;   // reject too-long path

    strcpy(req->method, method);
    strcpy(req->path, path);
    strcpy(req->version, version);
    
    if (!is_valid_path(req->path))
        return -1;

    bb_decode_percent(req->path, 0); // do NOT treat '+' as space in path
    if (strstr(req->path, ".."))
        return -1;

    // Query Params
    parse_query_params(req);

    return 0;
}

int bb_server_request_add_param(bb_server_request_t *req, const char *key, const char *value)
{
    if (req->param_count >= MAX_PARAMS)
       return -1;
    
    _bb_param_t *qp = &req->params[req->param_count];

    strncpy(qp->name, key, sizeof(qp->name) - 1);
    qp->name[sizeof(qp->name) - 1] = '\0';

    strncpy(qp->value, value, sizeof(qp->value) - 1);
    qp->value[sizeof(qp->value) - 1] = '\0';

    req->param_count++;
    return 0;
}

const char *bb_server_request_get_param(bb_server_request_t *req, const char *name)
{
    for (int i = 0; i < req->param_count; i++)
    {
        if (strcmp(req->params[i].name, name) == 0)
        {
            return req->params[i].value;
        }
    }
    return NULL;
}

int bb_server_request_add_query_param(bb_server_request_t *req, const char *key, const char *value)
{
    if (req->query_count >= MAX_QUERY_PARAMS)
       return -1;
    
    _bb_query_param_t *qp = &req->query[req->query_count];

    strncpy(qp->key, key, sizeof(qp->key) - 1);
    qp->key[sizeof(qp->key) - 1] = '\0';

    strncpy(qp->value, value, sizeof(qp->value) - 1);
    qp->value[sizeof(qp->value) - 1] = '\0';

    req->query_count++;
    return 0;
}

const char *bb_server_request_get_query_param(bb_server_request_t *req, const char *key)
{
    for (int i = 0; i < req->query_count; i++)
    {
        if (strcmp(req->query[i].key, key) == 0)
        {
            return req->query[i].value;
        }
    }
    return NULL;
}

int bb_server_request_set_method(bb_server_request_t *req, const char *method)
{
    if (!req || !method) {
        return -1;
    }

    size_t len = strlen(method);
    if (len >= METHOD_SIZE) {
        return -1;
    }

    memcpy(req->method, method, len + 1);
    return 0;
}

int bb_server_request_set_path(bb_server_request_t *req, const char *path)
{
    if (!req || !path) {
        return -1;
    }

    size_t len = strlen(path);
    if (len >= PATH_SIZE) {
        return -1;
    }

    memcpy(req->path, path, len + 1);
    return 0;
}
