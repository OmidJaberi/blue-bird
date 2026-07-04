#include "blue-bird/web/websocket/client.h"

#include "connection/connection.h"
#include "connection/async_tasks.h"
#include "http/parser.h"
#include "websocket/websocket_internal.h"

#include "blue-bird/runtime/event.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct bb_ws_client {
    bb_runtime_t *runtime;

    bb_websocket_t *websocket;

    bb_ws_handler_cb message_cb;
    void *message_userdata;
};

typedef struct {
    bb_ws_client_t *client;

    bb_ws_connect_cb connect_cb;
    void *connect_userdata;

    bb_connection_t *connection;

    char *host;
    char *path;
    int port;
} _bb_ws_client_task_data_t;

bb_ws_client_t *bb_ws_client_create_on_runtime(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_ws_client_t *client = calloc(1, sizeof(*client));

    if (!client)
    {
        return NULL;
    }

    client->runtime = runtime;

    return client;
}

bb_ws_client_t *bb_ws_client_create(void)
{
    return bb_ws_client_create_on_runtime(bb_runtime_default());
}

void bb_ws_client_close(bb_ws_client_t *client)
{
    if (!client)
    {
        return;
    }

    if (client->websocket)
    {
        bb_websocket_destroy(client->websocket);
        client->websocket = NULL;
    }
}

void bb_ws_client_destroy(bb_ws_client_t *client)
{
    if (!client)
    {
        return;
    }

    bb_ws_client_close(client);

    free(client);
}

void bb_ws_client_set_message_callback(bb_ws_client_t *client, bb_ws_handler_cb callback, void *userdata)
{
    if (!client)
    {
        return;
    }

    client->message_cb = callback;
    client->message_userdata = userdata;
}

static int _parse_ws_url(const char *url, char **host, int *port, char **path)
{
    if (!url)
    {
        return -1;
    }

    const char *authority = strstr(url, "://");

    authority = authority ? authority + 3 : url;

    const char *slash = strchr(authority, '/');

    if (!slash)
    {
        slash = authority + strlen(authority);
        *path = strdup("/");
    }
    else
    {
        *path = strdup(slash);
    }

    const char *colon = NULL;

    for (const char *p = authority; p < slash; p++)
    {
        if (*p == ':')
        {
            colon = p;
            break;
        }
    }

    if (colon)
    {
        size_t len = colon - authority;

        *host = malloc(len + 1);

        memcpy(*host, authority, len);

        (*host)[len] = 0;

        *port = atoi(colon + 1);
    }
    else
    {
        size_t len = slash - authority;

        *host = malloc(len + 1);

        memcpy(*host, authority, len);

        (*host)[len] = 0;

        *port = 80;
    }

    return 0;
}

static bb_read_status_t _bb_ws_handshake_read_step(void *userdata)
{
    _bb_ws_client_task_data_t *data = userdata;
    bb_connection_t *conn = data->connection;

    if (!bb_http_message_complete(conn->buffer, conn->buffer_length))
    {
        return (bb_read_status_t){
            .result = BB_READ_MORE,
        };
    }

    conn->buffer_length = 0;

    data->client->websocket = bb_websocket_create(conn, BB_WEBSOCKET_CLIENT);

    data->connection = NULL;
    data->connect_cb(data->client->websocket, BB_SUCCESS(), data->connect_userdata);

    bb_websocket_create_read_task(data->client->runtime, data->client->websocket->connection, data->client->message_cb);

    free(data->host);
    free(data->path);
    free(data);

    return (bb_read_status_t){
        .result = BB_READ_DONE,
    };
}

static void _bb_ws_handshake_read_error(bb_error_t err, void *userdata)
{
    (void)err;

    _bb_ws_client_task_data_t *data = userdata;

    if (data->connection)
    {
        bb_connection_destroy(data->connection);
    }

    data->connect_cb(data->client->websocket, BB_ERROR(BB_ERR_NETWORK, "Handshake failed"), data->connect_userdata);

    free(data->host);
    free(data->path);
    free(data);
}

static void _bb_ws_handshake_write_done(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_ws_client_task_data_t *data = userdata;

    bb_connection_task_create_read(data->client->runtime, data->connection, _bb_ws_handshake_read_step, _bb_ws_handshake_read_error, data);
}

static void _bb_ws_handshake_write_failed(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_ws_client_task_data_t *data = userdata;

    if (data->connection)
    {
        bb_connection_destroy(data->connection);
    }

    data->connect_cb(data->client->websocket, BB_ERROR(BB_ERR_NETWORK, "Handshake write failed"), data->connect_userdata);

    free(data->host);
    free(data->path);
    free(data);
}

void bb_ws_client_connect_async(bb_ws_client_t *client, const char *url, bb_ws_connect_cb callback, void *userdata)
{
    char *host = NULL;
    char *path = NULL;
    int port = 80;

    if (_parse_ws_url(url, &host, &port, &path) < 0)
    {
        callback(client->websocket, BB_ERROR(BB_ERR_BAD_REQUEST, "Invalid websocket URL"), userdata);
        return;
    }

    char port_str[16];

    snprintf(port_str, sizeof(port_str), "%d", port);

    bb_connection_t *connection = bb_connection_connect_nonblocking(host, port_str);

    if (!connection)
    {
        callback(NULL, BB_ERROR(BB_ERR_NETWORK, "Connection failed"), userdata);

        free(host);
        free(path);

        return;
    }

    _bb_ws_client_task_data_t *data = calloc(1, sizeof(*data));

    data->client = client;
    data->connect_cb = callback;
    data->connect_userdata = userdata;
    data->connection = connection;
    data->host = host;
    data->path = path;
    data->port = port;

    const char *key = "dGhlIHNhbXBsZSBub25jZQ==";

    char request[1024];

    snprintf(
        request,
        sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        data->path,
        data->host,
        data->port,
        key);

    bb_connection_buffer_add(connection, strdup(request), strlen(request));

    bb_connection_task_create_write(client->runtime, connection, _bb_ws_handshake_write_done, _bb_ws_handshake_write_failed, data);
}
