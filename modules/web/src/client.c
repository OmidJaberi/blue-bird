#include "blue-bird/web/client.h"
#include "http/parser.h"
#include "connection.h"

#include <stdio.h>
#include <stdlib.h>

struct bb_client {
    bb_connection_t *connection;

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
    client->req = bb_request_client_create();
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
    client->connection = NULL;
    return client;
}

void bb_client_destroy(bb_client_t *client)
{
    if (!client) return;

    if (client->connection)
    {
        bb_connection_destroy(client->connection);
        client->connection = NULL;
    }

    if (client->req) bb_request_destroy(client->req);
    if (client->res) bb_response_destroy(client->res);
    free(client);
}

void bb_client_reset(bb_client_t *client)
{
    bb_request_reset(client->req);
    bb_response_reset(client->res);
}

bb_request_t *bb_client_get_request(bb_client_t *client)
{
    return client->req;
}

bb_response_t *bb_client_get_response(bb_client_t *client)
{
    return client->res;
}

bb_error_t bb_client_connect(bb_client_t *client)
{
    if (!client || !client->req)
    {
        return BB_ERROR(BB_ERR_UNKNOWN, "Invalid client or request");
    }

    const char *host = bb_request_get_host(client->req);
    int port = bb_request_get_port(client->req);

    if (!host || port <= 0)
    {
        return BB_ERROR(BB_ERR_UNKNOWN, "Invalid host or port");
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    /* create connection */
    bb_connection_t *new_conn = bb_connection_connect(host, port_str);
    if (!new_conn)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to create connection");
    }
    if (client->connection)
    {
        bb_connection_destroy(client->connection);
    }
    client->connection = new_conn;
    return BB_SUCCESS();
}

bb_error_t bb_client_send(bb_client_t *client)
{
    if (!client || !client->req)
        return BB_ERROR(BB_ERR_UNKNOWN,"Invalid client or request");

    if (!client->connection)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    bb_request_serialize(client->req, &client->connection->write_buffer, &client->connection->write_length);
    bb_connection_write(client->connection);
    return BB_SUCCESS();
}

bb_error_t bb_client_receive(bb_client_t *client)
{
    if (!client || !client->res)
        return BB_ERROR(BB_ERR_UNKNOWN, "Invalid client or response");

    if (!client->connection)
        return BB_ERROR(BB_ERR_UNKNOWN, "Client not connected");

    bb_connection_t *conn = client->connection;

    while (!bb_http_message_complete(conn->buffer, conn->buffer_length))
    {
        ssize_t n = bb_connection_read(conn);

        if (n < 0)
        {
            return BB_ERROR(BB_ERR_IO, "Read failed");
        }

        if (n == 0)
            break;
    }

    if (bb_response_parse(conn->buffer, client->res) != 0)
    {
        return BB_ERROR(BB_ERR_UNKNOWN, "Failed to parse response");
    }

    return BB_SUCCESS();
}

void bb_client_close(bb_client_t *client)
{
    if (!client) return;

    if (client->connection)
    {
        bb_connection_destroy(client->connection);
        client->connection = NULL;
    }
}
