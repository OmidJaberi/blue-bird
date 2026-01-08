#include "core/client.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

BBError http_client_connect(HttpClient *client, const char *host, int port)
{
    if (!client || !host)
        return BB_ERROR(BB_ERR_UNKNOWN, "Invalid client or host");

    client->sock_fd = -1;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0)
        return BB_ERROR(BB_ERR_UNKNOWN, gai_strerror(rc));

    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next)
    {
        int fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
            continue;

        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
        {
            client->sock_fd = fd;
            break;
        }

        close(fd);
    }

    freeaddrinfo(res);

    if (client->sock_fd < 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Failed to connect");

    return BB_SUCCESS();
}

BBError http_client_send(HttpClient *client, client_request_t *req)
{
    if (!client || !req)
        return BB_ERROR(BB_ERR_UNKNOWN,"Invalid client or request");

    if (client->sock_fd < 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    /* ---- Build request start line ---- */
    const char *method = req->method ? req->method : "GET";
    const char *url = req->url ? req->url : "/";
    
    char start_line[512];
    snprintf(start_line, sizeof(start_line),
             "%s %s HTTP/1.1\r\n", method, url);

    /* ---- Send start line ---- */
    send(client->sock_fd, start_line, strlen(start_line), 0);

    /* ---- Send headers ---- */
    for (int i = 0; i < req->msg.header_count; i++)
    {
        char header_line[1024];
        snprintf(header_line, sizeof(header_line),
                 "%s: %s\r\n",
                 req->msg.headers[i].name,
                 req->msg.headers[i].value);

        send(client->sock_fd, header_line, strlen(header_line), 0);
    }

    /* ---- End headers ---- */
    send(client->sock_fd, "\r\n", 2, 0);

    /* ---- Send body if any ---- */
    if (req->msg.body && req->msg.body_len > 0)
    {
        send(client->sock_fd, req->msg.body, req->msg.body_len, 0);
    }

    return BB_SUCCESS();
}

BBError http_client_receive(HttpClient *client, client_response_t *res)
{
    if (!client || !res)
        return BB_ERROR(BB_ERR_UNKNOWN, "Invalid client or response");

    if (client->sock_fd < 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    init_client_response(res);

    /* Very naive receive buffer (OK for now) */
    char buffer[8192];
    ssize_t total = 0;

    while (1)
    {
        ssize_t n = recv(client->sock_fd,
                         buffer + total,
                         sizeof(buffer) - total - 1,
                         0);
        if (n <= 0)
            break;

        total += n;
        if (total >= (ssize_t)sizeof(buffer) - 1)
            break;
    }

    buffer[total] = '\0';

    /* Delegate parsing */
    if (parse_client_response(buffer, res) != 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Failed to parse response");

    return BB_SUCCESS();
}

void http_client_close(HttpClient *client)
{
    if (!client) return;

    if (client->sock_fd >= 0)
    {
        close(client->sock_fd);
        client->sock_fd = -1;
    }
}

