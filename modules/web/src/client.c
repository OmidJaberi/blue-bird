#include "blue-bird/web/client.h"
#include "http/parser.h"
#include "connection.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

struct bb_client {
    int sock_fd;

    bb_request_t *req;
    bb_response_t *res;
};

bb_client_t *bb_client_create(void)
{
    bb_client_t *client = malloc(sizeof(bb_client_t));
    if (!client)
    {
        return NULL;
    }
    client->req = bb_request_create();
    if (!client->req)
    {
        free(client);
        return NULL;
    }
    client->res = bb_response_create();
    if (!client->res)
    {
        bb_request_destroy(client->req);
        free(client);
        return NULL;
    }
    return client;
}

void bb_client_destroy(bb_client_t *client)
{
    if (client->req) bb_request_destroy(client->req);
    if (client->res) bb_response_destroy(client->res);
    free(client);
}

bb_request_t *bb_client_get_request(bb_client_t *client)
{
    return client->req;
}

bb_response_t *bb_client_get_response(bb_client_t *client)
{
    return client->res;
}

bb_error_t bb_client_connect(bb_client_t *client, const char *host, int port)
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

bb_error_t bb_client_send(bb_client_t *client)
{
    if (!client || !client->req)
        return BB_ERROR(BB_ERR_UNKNOWN,"Invalid client or request");

    if (client->sock_fd < 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    /* ---- Build request start line ---- */
    const char *method = bb_request_get_method(client->req) ? bb_request_get_method(client->req) : "GET";
    const char *url = bb_request_get_url(client->req) ? bb_request_get_url(client->req) : "/";
    
    char start_line[512];
    snprintf(start_line, sizeof(start_line),
             "%s %s HTTP/1.1", method, url);

    // Temporary:
    char *message;
    size_t size;
    bb_message_set_start_line(bb_request_get_message(client->req), start_line);
    bb_message_serialize(bb_request_get_message(client->req), &message, &size);
    send_all(client->sock_fd, message, size);

    free(message);
    return BB_SUCCESS();
}

bb_error_t bb_client_receive(bb_client_t *client)
{
    if (!client || !client->res)
        return BB_ERROR(BB_ERR_UNKNOWN, "Invalid client or response");

    if (client->sock_fd < 0)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    bb_connection_t *connection = bb_connection_create(NULL, client->sock_fd);
    if (!connection)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
    }

    while (!bb_http_request_complete(connection->buffer, connection->buffer_length))
    {
        ssize_t n = bb_connection_read(connection);

        if (n < 0)
        {
            bb_connection_destroy(connection);

            return BB_ERROR(BB_ERR_IO, "Read failed");
        }

        if (n == 0)
            break;
    }

    if (bb_response_parse(connection->buffer, client->res) != 0)
    {
        bb_connection_destroy(connection);
        return BB_ERROR(BB_ERR_UNKNOWN, "Failed to parse response");
    }

    // Prevent double-close
    connection->client_fd = -1;
    bb_connection_destroy(connection);
    return BB_SUCCESS();
}

void bb_client_close(bb_client_t *client)
{
    if (!client) return;

    if (client->sock_fd >= 0)
    {
        close(client->sock_fd);
        client->sock_fd = -1;
    }
}
