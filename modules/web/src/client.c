#include "blue-bird/web/client.h"
#include "http/parser.h"
#include "connection/connection.h"
#include "connection/async_connection.h"
#include "client_internal.h"

#include "blue-bird/web/error.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    bb_client_t *client;
    bb_client_callback_t callback;
    void *userdata;
} _bb_client_task_data_t;

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
    client->async_conn = NULL;
    client->connection = NULL;
    client->runtime = runtime;
    return client;
}

void bb_client_destroy(bb_client_t *client)
{
    if (!client) return;

    if (client->async_conn)
    {
        bb_async_connection_destroy(client->async_conn);
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

    if (client->async_conn)
    {
        bb_async_connection_destroy(client->async_conn);
        client->async_conn = NULL;
    }
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

    char *buffer;
    size_t length;
    bb_request_serialize(client->req, &buffer, &length);
    bb_connection_buffer_add(client->connection, buffer, length);
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
static void _client_read_error(bb_error_t err, void *userdata)
{
    _bb_client_task_data_t *data = userdata;

    data->callback(data->client, err, data->userdata);

    bb_client_close(data->client);
    free(data);
}

static bb_read_status_t _client_read_step(void *userdata)
{
    _bb_client_task_data_t *data = userdata;

    bb_client_t *client = data->client;
    bb_async_connection_t *async_conn = client->async_conn;
    bb_connection_t *conn = async_conn->connection;

    if (!bb_http_message_complete(conn->buffer, conn->buffer_length))
    {
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
    }

    if (bb_response_parse(conn->buffer, client->res))
    {
        return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Response parse failed") };
    }

    data->callback(client, BB_SUCCESS(), data->userdata);
    bb_client_close(client);
    free(data);

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

static void _client_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    _bb_client_task_data_t *data = userdata;
    bb_async_connection_create_read_task(data->client->async_conn, _client_read_step, _client_read_error, data);
}

static void _client_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    _bb_client_task_data_t *data = userdata;
    data->callback(data->client, BB_ERROR(BB_ERR_IO, "Write failed"), data->userdata);
    bb_client_close(data->client);
    free(data);
}

bb_error_t _client_create_write_task(bb_client_t *client, bb_client_callback_t callback, void *userdata)
{
    _bb_client_task_data_t *data = malloc(sizeof(*data));
    if (!data)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
    }

    data->client = client;
    data->callback = callback;
    data->userdata = userdata;

    char *buffer;
    size_t length;
    bb_request_serialize(client->req, &buffer, &length);
    bb_connection_buffer_add(client->async_conn->connection, buffer, length);

    return bb_async_connection_create_write_task(client->async_conn, _client_after_write, _client_write_error, data);
}

void bb_client_execute_async(bb_client_t *client, bb_client_callback_t callback, void *userdata)
{
    const char *host = bb_request_get_host(client->req);
    int port = bb_request_get_port(client->req);
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    client->async_conn = bb_async_connection_connect(client->runtime, host, port_str);
    if (!client->async_conn)
    {
        callback(client, BB_ERROR(BB_ERR_NETWORK, "Connection failed"), userdata);
        return;
    }

    bb_error_t err = _client_create_write_task(client, callback, userdata);
    if (BB_FAILED(err))
    {
        callback(client, err, userdata);
        return;
    }
}
