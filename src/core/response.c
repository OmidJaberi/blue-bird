#include "core/response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int create_response(Response *res, int status, const char *content)
{
    const char *status_text;
    switch (status)
    {
        case 200: status_text = "OK"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown"; break;
    }

    size_t content_len = strlen(content);

    res->body = malloc(content_len + 1);
    if (!res->body) return -1;
    strcpy(res->body, content);

    size_t header_len = snprintf(NULL, 0,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n",
        status, status_text, content_len);

    res->headers = malloc(header_len + 1);
    if (!res->headers)
    {
        free(res->body);
        return -1;
    }

    snprintf(res->headers, header_len + 1,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n",
        status, status_text, content_len);

    res->status = status;
    return 0;
}

int send_response(int sock_fd, Response *res)
{
    size_t headers_len = strlen(res->headers);
    size_t body_len = strlen(res->body);

    if (write(sock_fd, res->headers, headers_len) < 0) return -1;
    if (write(sock_fd, res->body, body_len) < 0) return -1;

    return 0;
}
