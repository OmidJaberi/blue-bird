#include "blue-bird/web/websocket/websocket.h"
#include "websocket/websocket_internal.h"
#include "connection/async_connection.h"

#include "blue-bird/runtime/event.h"
#include "blue-bird/error/error.h"
#include "blue-bird/utils/encoding.h"
#include "blue-bird/utils/hash.h"

#include "http/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

typedef struct {
    bb_websocket_t *ws;
    bb_ws_connect_cb connect_cb;
    void *connect_userdata;
} _bb_ws_client_task_data_t;

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
    bb_connection_t *conn = data->ws->async_conn->connection;
    if (!conn)
    {
        return (bb_read_status_t){
            .err = BB_ERROR(BB_ERR_NULL, "No connection"),
            .result = BB_READ_ERROR
        };
    }

    if (!bb_http_message_complete(conn->buffer, conn->buffer_length))
    {
        return (bb_read_status_t){
            .result = BB_READ_MORE,
        };
    }

    conn->buffer_length = 0;

    data->connect_cb(data->ws, BB_SUCCESS(), data->connect_userdata);

    bb_websocket_create_read_task(data->ws);

    free(data);

    return (bb_read_status_t){
        .result = BB_READ_DONE,
    };
}

static void _bb_ws_handshake_read_error(bb_error_t err, void *userdata)
{
    (void)err;

    _bb_ws_client_task_data_t *data = userdata;

    if (data->ws->async_conn->connection)
    {
        bb_connection_destroy(data->ws->async_conn->connection); // Close?
    }

    data->connect_cb(data->ws, BB_ERROR(BB_ERR_NETWORK, "Handshake failed"), data->connect_userdata);

    free(data);
}

static void _bb_ws_handshake_write_done(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_ws_client_task_data_t *data = userdata;

    bb_async_connection_create_read_task(data->ws->async_conn, _bb_ws_handshake_read_step, _bb_ws_handshake_read_error, data);
}

static void _bb_ws_handshake_write_failed(bb_task_t *task, void *userdata)
{
    (void)task;

    _bb_ws_client_task_data_t *data = userdata;

    if (data->ws->async_conn->connection)
    {
        bb_connection_destroy(data->ws->async_conn->connection); // Close?
    }

    data->connect_cb(data->ws, BB_ERROR(BB_ERR_NETWORK, "Handshake write failed"), data->connect_userdata);

    free(data);
}

void bb_websocket_connect(bb_websocket_t *ws, const char *url, bb_ws_connect_cb connect_callback, void *userdata)
{
    char *host = NULL;
    char *path = NULL;
    int port = 80;

    if (_parse_ws_url(url, &host, &port, &path) < 0)
    {
        connect_callback(ws, BB_ERROR(BB_ERR_BAD_REQUEST, "Invalid websocket URL"), userdata);
        return;
    }

    char port_str[16];

    snprintf(port_str, sizeof(port_str), "%d", port);

    bb_async_connection_t *async_conn = bb_async_connection_connect(ws->runtime, host, port_str);

    if (!async_conn || !async_conn->connection)
    {
        connect_callback(NULL, BB_ERROR(BB_ERR_NETWORK, "Connection failed"), userdata);

        free(host);
        free(path);

        return;
    }
    ws->async_conn = async_conn;

    _bb_ws_client_task_data_t *data = calloc(1, sizeof(*data));

    data->ws = ws;
    data->connect_cb = connect_callback;
    data->connect_userdata = userdata;

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
        path,
        host,
        port,
        key);

    bb_connection_buffer_add(async_conn->connection, strdup(request), strlen(request));

    bb_async_connection_create_write_task(ws->async_conn, _bb_ws_handshake_write_done, _bb_ws_handshake_write_failed, data);
}

bool _is_upgrade_request(bb_request_t *req)
{
    if (!req)
    {
        return false;
    }

    const char *upgrade = bb_request_get_header(req, "Upgrade");

    const char *connection = bb_request_get_header(req, "Connection");

    const char *key = bb_request_get_header(req, "Sec-WebSocket-Key");

    const char *version = bb_request_get_header(req, "Sec-WebSocket-Version");

    if (!upgrade || !connection || !key || !version)
    {
        return false;
    }

    if (strcasecmp(upgrade, "websocket") != 0)
    {
        return false;
    }

    if (strstr(connection, "Upgrade") == NULL && strstr(connection, "upgrade") == NULL)
    {
        return false;
    }

    if (strcmp(version, "13") != 0)
    {
        return false;
    }

    return true;
}

static const char BB_WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

char *bb_websocket_accept_key(const char *client_key)
{
    if (!client_key)
    {
        return NULL;
    }

    size_t key_len = strlen(client_key);
    size_t guid_len = strlen(BB_WS_GUID);

    char *combined = malloc(key_len + guid_len + 1);

    if (!combined)
    {
        return NULL;
    }

    memcpy(combined, client_key, key_len);

    memcpy(combined + key_len, BB_WS_GUID, guid_len + 1);

    unsigned char digest[BB_SHA1_DIGEST_LENGTH];

    bb_sha1(combined, key_len + guid_len, digest);

    free(combined);

    return bb_base64_encode(digest, BB_SHA1_DIGEST_LENGTH);
}

static bb_error_t bb_websocket_accept_req(bb_request_t *req, bb_response_t *res)
{
    if (!req || !res)
    {
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Invalid request or response");
    }

    if (!_is_upgrade_request(req))
    {
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Not a websocket upgrade request");
    }

    const char *client_key = bb_request_get_header(req, "Sec-WebSocket-Key");

    if (!client_key)
    {
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Missing Sec-WebSocket-Key");
    }

    char *accept_key = bb_websocket_accept_key(client_key);

    if (!accept_key)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to generate accept key");
    }

    bb_response_set_status(res, 101);

    bb_response_set_header(res, "Upgrade", "websocket");

    bb_response_set_header(res, "Connection", "Upgrade");

    bb_response_set_header(res, "Sec-WebSocket-Accept", accept_key);

    free(accept_key);

    return BB_SUCCESS();
}

bb_websocket_t *bb_websocket_accept(bb_async_connection_t *async_conn, bb_request_t *req, bb_response_t *res, bb_ws_handler_cb handler)
{
    if (!async_conn || !async_conn->connection)
        return NULL;
    bb_error_t err = bb_websocket_accept_req(req, res);
    async_conn->connection->buffer_length = 0; // Temporary Buffer reset

    if (BB_FAILED(err))
    {
        return NULL;
    }

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);
    ws->handler = handler;

    if (!ws)
    {
        return NULL;
        // return BB_ERROR(BB_ERR_ALLOC, "Failed to create websocket.");
    }
    return ws;
}

bb_websocket_t *bb_websocket_create_with_type(bb_async_connection_t *async_conn, bb_websocket_mode_t mode)
{
    if (!async_conn)
    {
        return NULL;
    }

    bb_websocket_t *ws = malloc(sizeof(*ws));

    if (!ws)
    {
        return NULL;
    }

    ws->runtime = async_conn->runtime;
    ws->async_conn = async_conn;
    ws->mode = mode;

    return ws;
}

bb_websocket_t *bb_websocket_create_on_runtime(bb_runtime_t *runtime)
{
    // return bb_websocket_create_with_type(runtime, NULL, BB_WEBSOCKET_CLIENT);
    if (!runtime)
    {
        return NULL;
    }

    bb_websocket_t *ws = malloc(sizeof(*ws));

    if (!ws)
    {
        return NULL;
    }

    ws->runtime = runtime;
    ws->async_conn = NULL;
    ws->mode = BB_WEBSOCKET_CLIENT;

    return ws;
}

void bb_websocket_destroy(bb_websocket_t *ws)
{
    if (!ws) return;
    bb_async_connection_destroy(ws->async_conn);
    free(ws);
}

void bb_websocket_set_message_callback(bb_websocket_t *ws, bb_ws_handler_cb callback, void *userdata)
{
    if (!ws)
    {
        return;
    }

    ws->handler = callback;
    ws->message_userdata = userdata;
}

void bb_websocket_set_pong_callback(bb_websocket_t *ws, bb_ws_pong_cb callback, void *userdata)
{
    if (!ws)
    {
        return;
    }

    ws->pong_cb = callback;
    ws->pong_userdata = userdata;
}

bb_error_t bb_websocket_read_frames(bb_websocket_t *ws, bb_ws_frame_t *frame)
{
    if (!ws || !frame)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    bb_connection_t *conn = ws->async_conn->connection;
    if (!conn)
    {
        return BB_ERROR(BB_ERR_NETWORK, "Connection doesn't exist.");
    }

    memset(frame, 0, sizeof(*frame));

    bb_ws_frame_t *current = frame;
    size_t consumed_total = 0;
    bool first = true;

    while (1)
    {
        size_t available = conn->buffer_length - consumed_total;

        if (available < 2)
        {
            break;
        }

        if (!first)
        {
            current->next = calloc(1, sizeof(*current));

            if (!current->next)
            {
                bb_ws_frame_destroy(frame);
                return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
            }

            current = current->next;
        }

        first = false;

        uint8_t *buf = (uint8_t *)conn->buffer + consumed_total;

        current->fin = (buf[0] >> 7) & 1;
        current->opcode = buf[0] & 0x0F;

        current->masked = (buf[1] >> 7) & 1;

        uint64_t payload_len = buf[1] & 0x7F;
        size_t offset = 2;

        if (payload_len == 126)
        {
            if (available < 4)
            {
                if (current != frame)
                {
                    bb_ws_frame_t *prev = frame;
                    while (prev->next != current)
                        prev = prev->next;
                    prev->next = NULL;
                    free(current);
                }
                break;
            }

            payload_len =
                ((uint16_t)buf[2] << 8) |
                ((uint16_t)buf[3]);

            offset += 2;
        }
        else if (payload_len == 127)
        {
            if (available < 10)
            {
                if (current != frame)
                {
                    bb_ws_frame_t *prev = frame;
                    while (prev->next != current)
                        prev = prev->next;
                    prev->next = NULL;
                    free(current);
                }
                break;
            }

            payload_len =
                ((uint64_t)buf[2] << 56) |
                ((uint64_t)buf[3] << 48) |
                ((uint64_t)buf[4] << 40) |
                ((uint64_t)buf[5] << 32) |
                ((uint64_t)buf[6] << 24) |
                ((uint64_t)buf[7] << 16) |
                ((uint64_t)buf[8] << 8)  |
                ((uint64_t)buf[9]);

            offset += 8;
        }

        current->payload_length = payload_len;

        if (current->masked)
        {
            if (available < offset + 4)
            {
                if (current != frame)
                {
                    bb_ws_frame_t *prev = frame;
                    while (prev->next != current)
                        prev = prev->next;
                    prev->next = NULL;
                    free(current);
                }
                break;
            }

            memcpy(current->masking_key, buf + offset, 4);
            offset += 4;
        }

        if (available < offset + payload_len)
        {
            if (current != frame)
            {
                bb_ws_frame_t *prev = frame;
                while (prev->next != current)
                    prev = prev->next;
                prev->next = NULL;
                free(current);
            }
            break;
        }

        current->payload = malloc(payload_len + 1);

        if (!current->payload)
        {
            bb_ws_frame_destroy(frame);

            return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
        }

        memcpy(current->payload, buf + offset, payload_len);

        if (current->masked)
        {
            for (uint64_t i = 0; i < payload_len; ++i)
            {
                current->payload[i] ^= current->masking_key[i & 3];
            }
        }

        current->payload[payload_len] = '\0';

        consumed_total += offset + payload_len;
    }

    if (consumed_total == 0)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Incomplete frame");
    }

    size_t remaining = conn->buffer_length - consumed_total;

    if (remaining > 0)
    {
        memmove(conn->buffer, conn->buffer + consumed_total, remaining);
    }

    conn->buffer_length = remaining;

    return BB_SUCCESS();
}

bb_error_t bb_websocket_queue_frame(bb_websocket_t *ws, const bb_ws_frame_t *frame)
{
    if (!ws || !frame)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    bb_connection_t *conn = ws->async_conn->connection;
    if (!conn)
    {
        return BB_ERROR(BB_ERR_NETWORK, "Connection doesn't exist.");
    }

    const int masked = (ws->mode == BB_WEBSOCKET_CLIENT);

    size_t ext_len = (frame->payload_length < 126) ? 0 : 2;
    size_t size = 2 + ext_len + (masked ? 4 : 0) + frame->payload_length;

    uint8_t *buffer = malloc(size);

    if (!buffer)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
    }

    uint8_t *out = buffer;
    size_t pos = 0;

    /* FIN + opcode */
    out[pos++] = 0x80 | (frame->opcode & 0x0F);

    /* Length + MASK bit */
    if (frame->payload_length < 126)
    {
        out[pos++] =
            (masked ? 0x80 : 0x00) |
            (uint8_t)frame->payload_length;
    }
    else
    {
        out[pos++] = (masked ? 0x80 : 0x00) | 126;
        out[pos++] = (frame->payload_length >> 8) & 0xff;
        out[pos++] = frame->payload_length & 0xff;
    }

    if (masked)
    {
        uint8_t mask[4];

        mask[0] = rand() & 0xff;
        mask[1] = rand() & 0xff;
        mask[2] = rand() & 0xff;
        mask[3] = rand() & 0xff;

        memcpy(out + pos, mask, 4);
        pos += 4;

        for (size_t i = 0; i < frame->payload_length; i++)
        {
            out[pos + i] =
                ((const uint8_t *)frame->payload)[i] ^
                mask[i & 3];
        }

        pos += frame->payload_length;
    }
    else
    {
        memcpy(out + pos, frame->payload, frame->payload_length);
        pos += frame->payload_length;
    }

    int rc = bb_connection_buffer_add(conn, (char *)buffer, pos);
    if (rc != 0)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed.");
    }

    return BB_SUCCESS();
}

bb_error_t bb_websocket_queue_text(bb_websocket_t *ws, const char *text)
{
    bb_ws_frame_t frame = {0};

    frame.fin = 1;
    frame.opcode = BB_WS_TEXT;

    frame.payload = (char *)text;
    frame.payload_length = strlen(text);

    return bb_websocket_queue_frame(ws, &frame);
}

bb_error_t bb_websocket_queue_binary(bb_websocket_t *ws, const void *data, size_t length)
{
    if (!data && length > 0)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid payload");
    }

    bb_ws_frame_t frame = {0};

    frame.fin = 1;
    frame.opcode = BB_WS_BINARY;

    frame.payload = (char *)data;
    frame.payload_length = length;

    return bb_websocket_queue_frame(ws, &frame);
}

static bb_error_t _bb_websocket_queue_control(bb_websocket_t *ws, bb_ws_opcode_t opcode, const void *payload, size_t length)
{
    bb_ws_frame_t frame = {0};

    frame.fin = 1;
    frame.opcode = opcode;

    frame.payload = (char *)payload;
    frame.payload_length = length;

    return bb_websocket_queue_frame(ws, &frame);
}

bb_error_t bb_websocket_queue_ping(bb_websocket_t *ws, const void *payload, size_t length)
{
    return _bb_websocket_queue_control(ws, BB_WS_PING, payload, length);
}

bb_error_t bb_websocket_queue_pong(bb_websocket_t *ws, const void *payload, size_t length)
{
    return _bb_websocket_queue_control(ws, BB_WS_PONG, payload, length);
}

bb_error_t bb_websocket_queue_close(bb_websocket_t *ws, uint16_t code, const char *reason)
{
    uint8_t payload[125];
    size_t length = 2;

    uint16_t network = htons(code);

    memcpy(payload, &network, sizeof(network));

    if (reason)
    {
        size_t reason_length = strlen(reason);

        if (reason_length > 123)
        {
            reason_length = 123;
        }

        memcpy(payload + 2, reason, reason_length);

        length += reason_length;
    }
    return _bb_websocket_queue_control(ws, BB_WS_CLOSE, payload, length);
}

bb_error_t bb_websocket_send_text(bb_websocket_t *ws, const char *text)
{
    if (!ws || !ws->async_conn || !ws->async_conn->connection)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Websocket not connected");
    }

    bb_error_t err = bb_websocket_queue_text(ws, text);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(ws->async_conn->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    return BB_SUCCESS();
}

bb_error_t bb_websocket_send_binary(bb_websocket_t *ws, const void *data, size_t length)
{
    if (!ws || !ws->async_conn || !ws->async_conn->connection)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Websocket not connected");
    }

    bb_error_t err = bb_websocket_queue_binary(ws, data, length);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(ws->async_conn->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    return BB_SUCCESS();
}

static bb_error_t bb_websocket_send_pong(bb_websocket_t *ws, const void *payload, size_t length)
{
    if (!ws || !ws->async_conn || !ws->async_conn->connection)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Websocket not connected");
    }

    bb_error_t err = bb_websocket_queue_pong(ws, payload, length);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(ws->async_conn->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    return BB_SUCCESS();
}

bb_error_t bb_websocket_send_ping(bb_websocket_t *ws, const void *payload, size_t length)
{
    if (!ws || !ws->async_conn || !ws->async_conn->connection)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Websocket not connected");
    }

    bb_error_t err = bb_websocket_queue_ping(ws, payload, length);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(ws->async_conn->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    return BB_SUCCESS();
}

bb_error_t bb_websocket_send_close(bb_websocket_t *ws, uint16_t code, const char *reason)
{
    if (!ws || !ws->async_conn || !ws->async_conn->connection)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Websocket not connected");
    }

    bb_error_t err = bb_websocket_queue_close(ws, code, reason);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(ws->async_conn->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    bb_async_connection_close(ws->async_conn);

    return BB_SUCCESS();
}

static void _websocket_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_websocket_t *ws = userdata;
    bb_error_t err = bb_websocket_create_read_task(ws);
    if (BB_FAILED(err))
    {
        bb_async_connection_close(ws->async_conn);
    }
}

static void _websocket_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_websocket_t *ws = userdata;
    bb_async_connection_close(ws->async_conn);
}

static void _websocket_read_error(bb_error_t err, void *userdata)
{
    (void)err;
    bb_websocket_t *ws = userdata;
    bb_async_connection_close(ws->async_conn);
}

static bb_read_status_t _websocket_read_step(void *userdata)
{
    bb_websocket_t *ws = userdata;

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(ws, &frame);

    if (err.code == BB_ERR_INTERNAL)
    {
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
    }

    if (BB_FAILED(err))
    {
        return (bb_read_status_t){ BB_READ_ERROR, err };
    }

    for (bb_ws_frame_t *current = &frame; current; current = current->next)
    {
        switch (current->opcode)
        {
            case BB_WS_TEXT:
            case BB_WS_BINARY:
            {
                bb_ws_message_t msg;

                err = bb_ws_frame_to_message(current, &msg);

                if (BB_FAILED(err))
                {
                    bb_ws_frame_destroy(&frame);
                    return (bb_read_status_t){ BB_READ_ERROR, err }; // Next frames ignored?
                }

                if (ws->handler)
                {
                    ws->handler(ws, &msg);
                }

                break;
            }
            case BB_WS_PING:
                bb_websocket_send_pong(ws, current->payload, current->payload_length);
                break;
            case BB_WS_PONG:
                if (ws->pong_cb)
                {
                    ws->pong_cb(ws, current->payload, current->payload_length, ws->pong_userdata);
                }
                break;
            case BB_WS_CLOSE:
            {
                bb_websocket_send_close(ws, 1000, NULL);
                bb_ws_frame_destroy(&frame);
                return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
            }
        }
    }

    bb_ws_frame_destroy(&frame);

    if (ws->async_conn && ws->async_conn->connection && ws->async_conn->connection->write_data && ws->async_conn->connection->write_data->write_buffer)
    {
        if (BB_FAILED(bb_async_connection_create_write_task(ws->async_conn, _websocket_after_write, _websocket_write_error, ws)))
        {
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") };
        }
    }

    return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
}

bb_error_t bb_websocket_create_read_task(bb_websocket_t *ws)
{
    if (!ws)
    {
        return BB_ERROR(BB_ERR_NULL, "Null websocket.");
    }

    bb_error_t err = bb_async_connection_create_read_task(ws->async_conn, _websocket_read_step, _websocket_read_error, ws);
    if (BB_FAILED(err))
    {
        bb_async_connection_close(ws->async_conn);
    }
    return err;
}
