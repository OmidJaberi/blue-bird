#include "blue-bird/web/websocket/websocket.h"
#include "websocket/websocket_internal.h"
#include "websocket/session.h"
#include "connection/async_tasks.h"

#include "blue-bird/error/error.h"
#include "blue-bird/utils/encoding.h"
#include "blue-bird/utils/hash.h"

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


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

bb_error_t bb_websocket_accept(bb_request_t *req, bb_response_t *res)
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

bb_websocket_t *bb_websocket_create(bb_connection_t *connection, bb_websocket_mode_t mode)
{
    if (!connection)
    {
        return NULL;
    }

    bb_websocket_t *ws = malloc(sizeof(*ws));

    if (!ws)
    {
        return NULL;
    }

    ws->connection = connection;
    ws->mode = mode;

    return ws;
}

void bb_websocket_destroy(bb_websocket_t *ws)
{
    free(ws);
}

bb_error_t bb_websocket_read_frames(bb_websocket_t *ws, bb_ws_frame_t *frame)
{
    if (!ws || !frame)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    bb_connection_t *conn = ws->connection;

    frame->next = NULL;

    bb_ws_frame_t *current = frame;
    size_t consumed_total = 0;

    while (1)
    {
        size_t available = conn->buffer_length - consumed_total;

        if (available < 2)
        {
            break;
        }

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
                break;
            }

            memcpy(current->masking_key, buf + offset, 4);
            offset += 4;
        }

        if (available < offset + payload_len)
        {
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
                current->payload[i] ^= current->masking_key[i % 4];
            }
        }

        current->payload[payload_len] = '\0';
        current->next = NULL;

        consumed_total += offset + payload_len;

        available = conn->buffer_length - consumed_total;

        if (available < 2)
        {
            break;
        }

        current->next = calloc(1, sizeof(bb_ws_frame_t));

        if (!current->next)
        {
            bb_ws_frame_destroy(frame);

            return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
        }

        current = current->next;
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

    bb_connection_t *conn = ws->connection;

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

bb_error_t bb_websocket_queue_ping(bb_websocket_t *ws)
{
    return _bb_websocket_queue_control(ws, BB_WS_PING, NULL, 0);
}

bb_error_t bb_websocket_queue_pong(bb_websocket_t *ws)
{
    return _bb_websocket_queue_control(ws, BB_WS_PONG, NULL, 0);
}

bb_error_t bb_websocket_queue_close(bb_websocket_t *ws)
{
    return _bb_websocket_queue_control(ws, BB_WS_CLOSE, NULL, 0);
}

typedef struct {
    bb_runtime_t *runtime;
    bb_ws_session_t *ws_session;
} bb_ws_task_data_t;

static void _websocket_after_write(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_ws_task_data_t *data = userdata;
    bb_error_t err = bb_websocket_create_read_task(data->runtime, data->ws_session->connection, data->ws_session->handler);
    if (BB_FAILED(err))
    {
        bb_ws_session_destroy(data->ws_session);
    }
    free(data);
}

static void _websocket_write_error(bb_task_t *task, void *userdata)
{
    (void) task;
    bb_ws_task_data_t *data = userdata;
    bb_connection_destroy(data->ws_session->connection);
    free(data);
}

static void _websocket_read_error(bb_error_t err, void *userdata)
{
    (void)err;
    bb_ws_task_data_t *data = userdata;
    bb_ws_session_destroy(data->ws_session);
    free(data);
}

static bb_read_status_t _websocket_read_step(void *userdata)
{
    bb_ws_task_data_t *data = userdata;
    bb_ws_session_t *session = data->ws_session;

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(session->websocket, &frame);

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
        bb_ws_message_t msg;

        err = bb_ws_frame_to_message(current, &msg);

        if (BB_FAILED(err))
        {
            bb_ws_frame_destroy(&frame);
            return (bb_read_status_t){ BB_READ_ERROR, err };
        }

        session->handler(&session->context, &msg);
    }

    bb_ws_frame_destroy(&frame);

    if (session->connection->write_data && session->connection->write_data->write_buffer)
    {
        if (BB_FAILED(bb_connection_task_create_write(data->runtime, session->connection, _websocket_after_write, _websocket_write_error, data)))
        {
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule write task.") };
        }
    }
    else
    {
        if (BB_FAILED(bb_websocket_create_read_task(data->runtime, session->connection, session->handler)))
        {
            return (bb_read_status_t){ BB_READ_ERROR, BB_ERROR(BB_ERR_INTERNAL, "Couldn't schedule read task.") };
        }
    }

    return (bb_read_status_t){ BB_READ_DONE, BB_SUCCESS() };
}

bb_error_t bb_websocket_create_read_task(bb_runtime_t *runtime, bb_connection_t *connection, bb_ws_handler_cb handler)
{
    bb_ws_task_data_t *data = malloc(sizeof(*data));
    if (!data)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate.");
    }
    data->runtime = runtime;
    data->ws_session = bb_ws_session_create(connection, handler);
    if (!data->ws_session)
    {
        free(data);
        return BB_ERROR(BB_ERR_ALLOC, "Failed to allocate.");
    }

    bb_error_t err = bb_connection_task_create_read(runtime, connection, _websocket_read_step, _websocket_read_error, data);
    if (BB_FAILED(err))
    {
        bb_ws_session_destroy(data->ws_session);
        free(data);
    }
    return err;
}

bb_error_t bb_websocket_send_text(bb_websocket_t *ws, const char *text)
{
    if (!ws)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Websocket not connected");
    }

    bb_error_t err = bb_websocket_queue_text(ws, text);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(ws->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    return BB_SUCCESS();
}

bb_error_t bb_websocket_send_binary(bb_websocket_t *ws, const void *data, size_t length)
{
    if (!ws)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Websocket not connected");
    }

    bb_error_t err = bb_websocket_queue_binary(ws, data, length);

    if (BB_FAILED(err))
    {
        return err;
    }

    if (bb_connection_write(ws->connection) < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Write failed");
    }

    return BB_SUCCESS();
}
