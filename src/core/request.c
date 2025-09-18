#include "core/request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse_request(const char *raw, Request *req)
{
    if (!raw || !req) return -1;

    req->param_count = 0;
    req->body = NULL;
    req->body_len = 0;

    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;

    char line[512];
    size_t len = line_end - raw;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    strncpy(line, raw, len);
    line[len] = '\0';

    if (sscanf(line, "%7s %255s %15s", req->method, req->path, req->version) != 3)
    {
        return -1;
    }

    // TODO: Parse headers

    const char *body_start = strstr(raw, "\r\n\r\n");
    if (!body_start)
        return 0;

    body_start += 4; // skip "\r\n\r\n"
    size_t body_len = strlen(body_start);
    if (body_len == 0)
        return 0;

    req->body = (char *)malloc(body_len + 1);
    if (!req->body)
        return -1;

    memcpy(req->body, body_start, body_len);
    req->body[body_len] = '\0';
    req->body_len = body_len;

    return 0;
}

void destroy_request(Request *req)
{
    if (req->body)
    {
        free(req->body);
        req->body = NULL;
        req->body_len = 0;
    }
    req->param_count = 0;
}

const char *get_param(Request *req, const char *name)
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
