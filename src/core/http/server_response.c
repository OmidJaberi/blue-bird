#include "core/http/server_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void init_server_response(server_response_t *res)
{
    res->status_code = 200;
    res->status_text = strdup("OK");
    init_message(&res->msg);
}

void destroy_server_response(server_response_t *res)
{
    free(res->status_text);
    destroy_message(&res->msg);
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

int set_server_response_status(server_response_t *res, int code)
{
    res->status_code = code;
    res->status_text = strdup(status_text_for_code(code));
    if (!res->status_text)
    {
        return -1;
    }
    return 0;
}

void set_server_response_header(server_response_t *res, const char *name, const char *value)
{
    set_message_header(&res->msg, name, value);
}

void set_server_response_body(server_response_t *res, char *body)
{
    set_message_body(&res->msg, body);
}

int serialize_server_response(server_response_t *res, char *buffer, int buffer_size)
{
    char start_line_buff[128];
    snprintf(start_line_buff, 128,
                           "HTTP/1.1 %d %s",
                           res->status_code, res->status_text);
    set_message_start_line(&res->msg, start_line_buff);
    return serialize_message(&res->msg, buffer, buffer_size);
}

int send_server_response(int sock_fd, server_response_t *res)
{
    char outbuf[8192];
    int len = serialize_server_response(res, outbuf, sizeof(outbuf));

    if (write(sock_fd, outbuf, len) < 0)
        return -1;

    return 0;
}
