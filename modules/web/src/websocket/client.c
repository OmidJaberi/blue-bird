#include "blue-bird/web/websocket/client.h"

#include "connection.h"
#include "http/parser.h"
#include "websocket/websocket_internal.h"

#include "blue-bird/runtime/event.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct bb_ws_client {
    bb_runtime_t *runtime;

    bb_connection_t *connection;
    bb_websocket_t *websocket;

    bb_ws_message_cb message_cb;
    void *message_userdata;
};

typedef struct {
    bb_ws_client_t *client;

    bb_ws_connect_cb connect_cb;
    void *connect_userdata;

    char *host;
    char *path;
    int port;
} _bb_ws_client_task_data_t;

static void _bb_ws_connect_task(bb_task_t *task, void *userdata);

static void _bb_ws_handshake_read_task(bb_task_t *task, void *userdata);

static void _bb_ws_read_task(bb_task_t *task, void *userdata);

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

    if (client->connection)
    {
        bb_connection_destroy(client->connection);
        client->connection = NULL;
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

void bb_ws_client_set_message_callback(bb_ws_client_t *client, bb_ws_message_cb callback, void *userdata)
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

void bb_ws_client_connect_async(bb_ws_client_t *client, const char *url, bb_ws_connect_cb callback, void *userdata)
{
    char *host = NULL;
    char *path = NULL;
    int port = 80;

    if (_parse_ws_url(url, &host, &port, &path) < 0)
    {
        callback(client, BB_ERROR(BB_ERR_BAD_REQUEST, "Invalid websocket URL"), userdata);
        return;
    }

    char port_str[16];

    snprintf(port_str, sizeof(port_str), "%d", port);

    client->connection = bb_connection_connect(host, port_str);

    if (!client->connection)
    {
        callback(client, BB_ERROR(BB_ERR_NETWORK, "Connection failed"), userdata);

        free(host);
        free(path);

        return;
    }

    _bb_ws_client_task_data_t *data = calloc(1, sizeof(*data));

    data->client = client;
    data->connect_cb = callback;
    data->connect_userdata = userdata;
    data->host = host;
    data->path = path;
    data->port = port;

    bb_task_t *task = bb_task_create(_bb_ws_connect_task, data);

    bb_runtime_watch_fd(client->runtime, client->connection->fd, BB_EVENT_WRITE, BB_WATCH_ONESHOT, task);
}

static void _bb_ws_connect_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_ws_client_task_data_t *data = userdata;

    bb_connection_t *conn = data->client->connection;

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
        key
    );

    conn->write_buffer = strdup(request);

    conn->write_length = strlen(request);

    conn->write_offset = 0;

    bb_connection_write(conn);

    bb_task_t *next = bb_task_create(_bb_ws_handshake_read_task, data);

    bb_runtime_watch_fd(data->client->runtime, conn->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
}

static void _bb_ws_handshake_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_ws_client_task_data_t *data = userdata;

    bb_connection_t *conn = data->client->connection;

    if (bb_connection_read(conn) < 0)
    {
        goto fail;
    }

    if (!bb_http_message_complete(conn->buffer, conn->buffer_length))
    {
        bb_task_t *next = bb_task_create(_bb_ws_handshake_read_task, data);

        bb_runtime_watch_fd(data->client->runtime, conn->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);

        return;
    }

    data->client->websocket = bb_websocket_create(conn);

    data->connect_cb(data->client, BB_SUCCESS(), data->connect_userdata);

    bb_task_t *read_task = bb_task_create(_bb_ws_read_task, data->client);

    bb_runtime_watch_fd(data->client->runtime, conn->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, read_task);

    free(data->host);
    free(data->path);
    free(data);

    return;

fail:

    data->connect_cb(data->client, BB_ERROR(BB_ERR_NETWORK, "Handshake failed"), data->connect_userdata);

    free(data->host);
    free(data->path);
    free(data);
}

static void _bb_ws_read_task(bb_task_t *task, void *userdata)
{
    (void)task;

    bb_ws_client_t *client = userdata;

    if (bb_connection_read(client->connection) < 0)
    {
        return;
    }

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frame(client->websocket, &frame);

    if (err.code == BB_ERR_INTERNAL)
    {
        bb_task_t *next = bb_task_create(_bb_ws_read_task, client);

        bb_runtime_watch_fd(client->runtime, client->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);

        return;
    }

    if (BB_FAILED(err))
    {
        return;
    }

    bb_ws_message_t msg;

    err = bb_ws_frame_to_message(&frame, &msg);

    if (!BB_FAILED(err) && client->message_cb)
    {
        client->message_cb(client, &msg, client->message_userdata);
    }

    bb_ws_frame_destroy(&frame);

    bb_task_t *next = bb_task_create(_bb_ws_read_task, client);

    bb_runtime_watch_fd(client->runtime, client->connection->fd, BB_EVENT_READ, BB_WATCH_ONESHOT, next);
}

bb_error_t bb_ws_client_send_text(bb_ws_client_t *client, const char *text)
{
    if (!client || !client->websocket)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Client not connected");
    }

    bb_error_t err = bb_websocket_send_text(client->websocket, text);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(client->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    return BB_SUCCESS();
}
