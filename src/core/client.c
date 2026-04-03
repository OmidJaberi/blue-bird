#include "core/client.h"
#include "core/http.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

BBError http_client_connect(bb_client_t *client, const char *host, int port)
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

static ssize_t send_all(int fd, const void *data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, (char*)data + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return sent;
}

BBError http_client_send(bb_client_t *client, request_t *req)
{
    if (!client || !req)
        return BB_ERROR(BB_ERR_UNKNOWN,"Invalid client or request");

    if (client->sock_fd < 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    /* ---- Build request start line ---- */
    const char *method = GET_REQUEST_METHOD(*req) ? GET_REQUEST_METHOD(*req) : "GET";
    const char *url = GET_REQUEST_URL(*req) ? GET_REQUEST_URL(*req) : "/";
    
    char start_line[512];
    snprintf(start_line, sizeof(start_line),
             "%s %s HTTP/1.1", method, url);

    // Temporary:
    char *message;
    int size;
    set_message_start_line(&GET_REQUEST_MESSAGE(*req), start_line);
    serialize_message(&GET_REQUEST_MESSAGE(*req), &message, &size);
    send_all(client->sock_fd, message, size);

    free(message);
    return BB_SUCCESS();
}

BBError http_client_receive(bb_client_t *client, response_t *res)
{
    if (!client || !res)
        return BB_ERROR(BB_ERR_UNKNOWN, "Invalid client or response");

    if (client->sock_fd < 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    init_response(res);

    char *buffer;
    read_http_message(client->sock_fd, &buffer);

    /* Delegate parsing */
    if (parse_response(buffer, res) != 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Failed to parse response");

    free(buffer);
    return BB_SUCCESS();
}

void http_client_close(bb_client_t *client)
{
    if (!client) return;

    if (client->sock_fd >= 0)
    {
        close(client->sock_fd);
        client->sock_fd = -1;
    }
}

