#include "core/http/response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void init_response(response_t *res)
{
    res->status_code = 200;
    res->status_text = strdup("OK");
    res->headers = NULL;
    res->header_count = 0;
    res->body = NULL;
}

void destroy_response(response_t *res)
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

int set_status(response_t *res, int code)
{
    res->status_code = code;
    res->status_text = strdup(status_text_for_code(code));
    if (!res->status_text)
    {
        return -1;
    }
    return 0;
}

void set_header(response_t *res, const char *name, const char *value)
{
    set_message_header(&res->msg, name, value);
}

void set_body(response_t *res, char *body)
{
    free(res->body);
    res->body = strdup(body);
}

int serialize_response(response_t *res, char *buffer, int buffer_size)
{
    int written = snprintf(buffer, buffer_size,
                           "HTTP/1.1 %d %s\r\n",
                           res->status_code, res->status_text);
    
    for (int i = 0; i < res->header_count; i++)
        written += snprintf(buffer + written, buffer_size - written,
                "%s: %s\r\n",
                res->headers[i].name,
                res->headers[i].value);

    int body_len = res->body ? strlen(res->body) : 0;
    // Conent_Length added here:
    written += snprintf(buffer + written, buffer_size - written,
            "Content-Length: %d\r\n\r\n",
            body_len);

    if (res->body)
        written += snprintf(buffer + written, buffer_size - written,
                "%s", res->body);

    return written;
}

int send_response(int sock_fd, response_t *res)
{
    char outbuf[8192];
    int len = serialize_response(res, outbuf, sizeof(outbuf));

    if (write(sock_fd, outbuf, len) < 0)
        return -1;

    return 0;
}
