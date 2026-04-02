#include "core/http/server_request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_server_request(server_request_t *req)
{
    if (!req) return;
    init_message(&req->msg);
}

static int hexval(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *s, int decode_plus)
{
    char *src = s;
    char *dst = s;

    while (*src) {
        if (decode_plus && *src == '+') {
            *dst++ = ' ';
            src++;
        }
        else if (*src == '%' &&
                 hexval(src[1]) >= 0 &&
                 hexval(src[2]) >= 0)
        {
            int hi = hexval(src[1]);
            int lo = hexval(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        }
        else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static int parse_header(const char **raw, char **name_buf, char **value_buf)
{
    const char *colon = strchr(*raw, ':');
    if (!colon) return -1;  // Malformed Header (missing ':')

    const char *end = strstr(*raw, "\r\n");
    if (!end) return -1;    // Malformed Header (no CRLF)

    size_t name_len = colon - *raw;
    size_t value_len = end - (colon + 1);

    // copy name
    *name_buf = malloc(name_len + 1);
    if (!*name_buf) return -1;
    strncpy(*name_buf, *raw, name_len);
    (*name_buf)[name_len] = '\0';

    // Skip leading space in value
    const char *val_start = colon + 1;
    while (*val_start == ' ' && value_len > 0)
    {
        val_start++;
        value_len--;
    }
    // copy value
    *value_buf = malloc(value_len + 1);
    if (!*value_buf) return -1;
    strncpy(*value_buf, val_start, value_len);
    (*value_buf)[value_len] = '\0';

    *raw = end + 2; // next line
    return 0;
}

void parse_query_params(server_request_t *req)
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

            url_decode(pair, 1);      // '+' becomes space in query
            url_decode(eq + 1, 1);
            add_server_request_query_param(req, pair, eq + 1);

            if (eq)
            {
                *eq = '\0';
                add_server_request_query_param(req, pair, eq + 1);

            }
            else
                add_server_request_query_param(req, pair, "");
            pair = strtok(NULL, "&");
        }
    }
}

static int parse_body(server_request_t *req, const char *raw)
{
    const char *body_start = strstr(raw, "\r\n\r\n");
    if (!body_start) return 0;

    body_start += 4; // skip "\r\n\r\n"
    const char *content_length = get_server_request_header(req, "Content-Length");
    size_t body_len = content_length ? atoi(content_length) : strlen(body_start);

    if (body_len <= 0)
        return 0;

    char *body_buf = (char *)malloc(body_len + 1);
    if (!body_buf) return -1;

    memcpy(body_buf, body_start, body_len);
    body_buf[body_len] = '\0';

    const char *ctype = get_server_request_header(req, "Content-Type");
    if (ctype && strcasecmp(ctype, "application/x-www-form-urlencoded") == 0)
        url_decode(body_buf, 1);

    // Store into message
    set_message_body(&req->msg, body_buf);
    free(body_buf);

    return 0;
}

int parse_server_request(const char *raw, server_request_t *req)
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
    char method[8];
    char path[4096];
    char version[16];

    if (sscanf(line, "%7s %4095s %15s", method, path, version) != 3)
        return -1;

    if (strlen(path) >= 256)
        return -1;   // reject too-long path

    strcpy(req->method, method);
    strcpy(req->path, path);
    strcpy(req->version, version);

    url_decode(req->path, 0); // do NOT treat '+' as space in path

    // Parse headers
    const char *header_start = line_end + 2;

    while (*header_start && !(header_start[0] == '\r' && header_start[1] == '\n'))
    {
        char *name_buf = NULL;
        char *value_buf = NULL;

        if (parse_header(&header_start, &name_buf, &value_buf) < 0)
        {
            if (name_buf) free(name_buf);
            if (value_buf) free(value_buf);
            return -1;
        }

        set_message_header(&req->msg, name_buf, value_buf);

        if (name_buf) free(name_buf);
        if (value_buf) free(value_buf);
    }

    // Query Params
    parse_query_params(req);

    // Parse Body
    return parse_body(req, raw);
}

void destroy_server_request(server_request_t *req)
{
    destroy_message(&req->msg);
    req->param_count = 0;
}

const char *get_server_request_param(server_request_t *req, const char *name)
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

int add_server_request_query_param(server_request_t *req, const char *key, const char *value)
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

const char *get_server_request_query_param(server_request_t *req, const char *key)
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

const char *get_server_request_header(server_request_t *req, const char *name)
{
    return get_message_header(&req->msg, name);
}
