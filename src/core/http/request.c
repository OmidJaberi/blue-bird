#include "core/http/request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse_request(const char *raw, request_t *req)
{
    if (!raw || !req) return -1;

    req->param_count = 0;
    req->query_count = 0;
    init_message(&req->msg);

    // Parse request line
    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;

    char line[512];
    size_t len = line_end - raw;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    strncpy(line, raw, len);
    line[len] = '\0';

    // Set the start line in http_message_t
    set_message_start_line(&req->msg, line);

    // Parse method, path, version
    if (sscanf(line, "%7s %255s %15s", req->method, req->path, req->version) != 3)
    {
        return -1;
    }

    // Parse headers
    const char *headers_start = line_end + 2;
    const char *p = headers_start;

    while (*p && !(p[0] == '\r' && p[1] == '\n'))
    {
        const char *colon = strchr(p, ':');
        if (!colon) return -1;  // Malformed Header (missing ':')

        const char *end = strstr(p, "\r\n");
        if (!end) return -1;    // Malformed Header (no CRLF)

        size_t name_len = colon - p;
        size_t value_len = end - (colon + 1);

        // SET HEADERS
        char name_buf[128];
        char value_buf[512];

        // copy name
        if (name_len >= sizeof(name_buf)) name_len = sizeof(name_buf) - 1;
        strncpy(name_buf, p, name_len);
        name_buf[name_len] = '\0';

        // Skip leading space in value
        const char *val_start = colon + 1;
        while (*val_start == ' ' && value_len > 0)
        {
            val_start++;
            value_len--;
        }
        // copy value
        if (value_len >= sizeof(value_buf)) value_len = sizeof(value_buf) - 1;
        strncpy(value_buf, val_start, value_len);
        value_buf[value_len] = '\0';

        // set into http_message_t
        set_message_header(&req->msg, name_buf, value_buf);


        p = end + 2; // next line
    }

    // Query Params
    char *qmark = strchr(req->path, '?');
    if (qmark)
    {
        *qmark = '\0';
        char *query_str = qmark + 1;
        char *pair = strtok(query_str, "&");
        while (pair)
        {
            char *eq = strchr(pair, '=');
            if (eq)
            {
                *eq = '\0';
                add_query_param(req, pair, eq + 1);

            }
            else
                add_query_param(req, pair, "");
            pair = strtok(NULL, "&");
        }
    }

    // Parse Body
    const char *body_start = strstr(raw, "\r\n\r\n");
    if (!body_start) return 0;

    body_start += 4; // skip "\r\n\r\n"
    const char *content_length = get_header(req, "Content-Length");
    size_t body_len = content_length ? atoi(content_length) : strlen(body_start);

    if (body_len <= 0)
        return 0;

    char *body_buf = (char *)malloc(body_len + 1);
    if (!body_buf) return -1;

    memcpy(body_buf, body_start, body_len);
    body_buf[body_len] = '\0';

    // Store into message
    set_message_body(&req->msg, body_buf);
    free(body_buf);

    return 0;
}

void destroy_request(request_t *req)
{
    destroy_message(&req->msg);
    req->param_count = 0;
}

const char *get_param(request_t *req, const char *name)
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

int add_query_param(request_t *req, const char *key, const char *value)
{
    if (req->query_count >= MAX_QUERY_PARAMS)
       return -1;
    
    query_param_t *qp = &req->query[req->query_count];

    strncpy(qp->key, key, sizeof(qp->key) - 1);
    qp->key[sizeof(qp->key) - 1] = '\0';

    strncpy(qp->value, value, sizeof(qp->value) - 1);
    qp->value[sizeof(qp->value) - 1] = '\0';

    req->query_count++;
    return 0;
}

const char *get_query_param(request_t *req, const char *key)
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

const char *get_header(request_t *req, const char *name)
{
    return get_message_header(&req->msg, name);
}
