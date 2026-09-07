#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blue-bird/web/http/response.h"
#include "blue-bird/web/http/message.h"
#include "http/response.h"

struct bb_response {
    bb_http_message_t *msg;
    int status_code;
};

static char *status_text_for_code(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

bb_response_t *bb_response_create(void)
{
    bb_response_t *res = malloc(sizeof(bb_response_t));
    if (res != NULL)
    {
        res->status_code = 200;
        res->msg = bb_message_create();
    }
    return res;
}

void bb_response_destroy(bb_response_t *res)
{
    bb_message_destroy(res->msg);
    free(res);
}

void bb_response_reset(bb_response_t *res)
{
    if (!res)
        return;
    res->status_code = 200;
    if (!res->msg)
    {
        res->msg = bb_message_create();
    }
    else
    {
        bb_message_reset(res->msg);
    }
}

int bb_response_set_status(bb_response_t *res, int code)
{
    res->status_code = code;
    return 0;
}

int bb_response_get_status(bb_response_t *res)
{
    if (!res)
    {
        return -1;
    }
    return res->status_code;
}

int bb_response_serialize(bb_response_t *res, char **buffer, size_t *buffer_size)
{
    char start_line_buff[128];
    snprintf(start_line_buff, 128, "HTTP/1.1 %d %s", res->status_code, status_text_for_code(res->status_code));
    bb_message_set_start_line(res->msg, start_line_buff);
    return bb_message_serialize(res->msg, buffer, buffer_size);
}

int bb_response_parse(const char *raw, bb_response_t *res)
{
    if (!raw || !res)
        return -1;

    if (bb_message_parse(raw, res->msg) != 0)
        return -1;

    /* HTTP/1.1 200 OK */
    sscanf(bb_message_get_start_line(res->msg), "HTTP/%*s %d", &res->status_code);

    return 0;
}

bb_http_message_t *bb_response_get_message(bb_response_t *res)
{
    return res->msg;
}

int bb_response_from_parsed(const bb_http_response_t *parsed, bb_response_t *res)
{
    if (!parsed || !res || !res->msg) return -1;

    bb_response_reset(res);
    res->status_code = parsed->status_code;

    char start_line[BB_HTTP_MAX_REQUEST_LINE + 32];
    snprintf(start_line, sizeof(start_line), "HTTP/%d.%d %d%s%s",
             parsed->version_major, parsed->version_minor, parsed->status_code,
             parsed->reason[0] ? " " : "", parsed->reason);
    bb_message_set_start_line(res->msg, start_line);

    for (size_t i = 0; i < parsed->header_count; i++)
        bb_message_set_header(res->msg, parsed->headers[i].name, parsed->headers[i].value);

    bb_message_set_body_data(res->msg, parsed->body, parsed->body_len);
    return 0;
}
