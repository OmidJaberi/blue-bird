#include "blue-bird/web/client.h"
#include "http/parser.h"
#include "connection.h"
#include "async_connection.h"
#include "client_internal.h"

#include <stdio.h>
#include <stdlib.h>

bb_client_t *bb_client_create_on_runtime(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return NULL;
    }
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
    client->runtime = runtime;
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
    if (!client)
    {
        return;
    }
    bb_client_close(client);
    bb_request_reset(client->req);
    bb_response_reset(client->res);
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

bb_request_t *bb_client_get_request(bb_client_t *client)
{
    return client->req;
}

bb_response_t *bb_client_get_response(bb_client_t *client)
{
    return client->res;
}

// Sync Client:
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
        return BB_ERROR(BB_ERR_NETWORK, "Failed to create connection");
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
    ssize_t rc = bb_connection_write(client->connection);
    if (rc < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }
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

// Async Client:
void bb_client_execute_async(bb_client_t *client, bb_client_callback_t callback, void *userdata)
{
    const char *host = bb_request_get_host(client->req);
    int port = bb_request_get_port(client->req);
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    client->connection = bb_connection_connect(host, port_str);
    if (!client->connection)
    {
        callback(client, BB_ERROR(BB_ERR_NETWORK, "Connection failed"), userdata);
        return;
    }

    _bb_client_task_data_t *data = malloc(sizeof(*data));
    if (!data)
    {
        callback(client, BB_ERROR(BB_ERR_ALLOC, "Allocation failed"), userdata);
        return;
    }

    data->client = client;
    data->callback = callback;
    data->userdata = userdata;

    bb_task_t *task = bb_task_create(bb_client_write_task, data);

    bb_runtime_watch_fd(client->runtime, client->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
}
