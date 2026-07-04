#include "blue-bird/web/websocket/client.h"

#include "connection/connection.h"
#include "connection/async_tasks.h"
#include "http/parser.h"
#include "websocket/websocket_internal.h"

#include "blue-bird/runtime/event.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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
    if (!url || !host || !port || !path)
        return -1;

    *host = NULL;
    *path = NULL;
    *port = 0;

    const char *scheme = strstr(url, "://");
    const char *authority = scheme ? scheme + 3 : url;

    int default_port = 80;

    if (scheme)
    {
        size_t scheme_len = (size_t)(scheme - url);

        if (scheme_len == 3 && strncmp(url, "wss", 3) == 0)
            default_port = 443;
        else if (scheme_len != 2 || strncmp(url, "ws", 2) != 0)
            return -1;
    }

    /* Find beginning of path */
    const char *slash = strchr(authority, '/');

    if (slash)
    {
        *path = strdup(slash);
    }
    else
    {
        slash = authority + strlen(authority);
        *path = strdup("/");
    }

    if (!*path)
        return -1;

    const char *host_begin;
    const char *host_end;
    const char *port_begin = NULL;

    if (*authority == '[')
    {
        /* IPv6 */
        const char *rb = memchr(authority, ']', (size_t)(slash - authority));
        if (!rb)
            goto error;

        host_begin = authority + 1;
        host_end = rb;

        if (rb + 1 < slash)
        {
            if (rb[1] != ':')
                goto error;

            port_begin = rb + 2;
        }
    }
    else
    {
        host_begin = authority;

        const char *colon = memchr(authority, ':', (size_t)(slash - authority));

        if (colon)
        {
            host_end = colon;
            port_begin = colon + 1;
        }
        else
        {
            host_end = slash;
        }
    }

    if (host_end <= host_begin)
        goto error;

    size_t host_len = (size_t)(host_end - host_begin);

    *host = malloc(host_len + 1);

    if (!*host)
        goto error;

    memcpy(*host, host_begin, host_len);
    (*host)[host_len] = '\0';

    if (port_begin)
    {
        size_t port_len = (size_t)(slash - port_begin);

        if (port_len == 0 || port_len > 5)
            goto error;

        char port_buf[6];

        memcpy(port_buf, port_begin, port_len);
        port_buf[port_len] = '\0';

        char *endptr;

        errno = 0;
        long p = strtol(port_buf, &endptr, 10);

        if (errno || *endptr != '\0' || p < 1 || p > 65535)
            goto error;

        *port = (int)p;
    }
    else
    {
        *port = default_port;
    }

    return 0;

error:
    free(*host);
    free(*path);

    *host = NULL;
    *path = NULL;
    *port = 0;

    return -1;
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
