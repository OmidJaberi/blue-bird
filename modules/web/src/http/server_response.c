#include "blue-bird/web/http/server_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void bb_server_response_init(bb_server_response_t *res)
{
    res->status_code = 200;
    res->status_text = strdup("OK");
    bb_message_init(&res->msg);
}

void bb_server_response_destroy(bb_server_response_t *res)
{
    free(res->status_text);
    bb_message_destroy(&res->msg);
}

char *status_text_for_code(int code)
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

int bb_server_response_set_status(bb_server_response_t *res, int code)
{
    res->status_code = code;
    res->status_text = strdup(status_text_for_code(code));
    if (!res->status_text)
    {
        return -1;
    }
    return 0;
}

void bb_server_response_set_header(bb_server_response_t *res, const char *name, const char *value)
{
    bb_message_set_header(&res->msg, name, value);
}

void bb_server_response_set_body(bb_server_response_t *res, char *body)
{
    bb_message_set_body(&res->msg, body);
}

int bb_server_response_serialize(bb_server_response_t *res, char **buffer, int *buffer_size)
{
    char start_line_buff[128];
    snprintf(start_line_buff, 128,
                           "HTTP/1.1 %d %s",
                           res->status_code, res->status_text);
    bb_message_set_start_line(&res->msg, start_line_buff);
    return bb_message_serialize(&res->msg, buffer, buffer_size);
}
