#include "websocket/websocket_internal.h"
#include "websocket/session.h"

#include "blue-bird/error/error.h"

#include "async_connection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

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

bb_error_t bb_websocket_read_frame(bb_websocket_t *ws, bb_ws_frame_t *frame)
{
    if (!ws || !frame)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    bb_connection_t *conn = ws->connection;

    if (conn->buffer_length < 2)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Incomplete frame");
    }

    uint8_t *buf = (uint8_t *)conn->buffer;

    frame->fin = (buf[0] >> 7) & 1;
    frame->opcode = buf[0] & 0x0F;

    frame->masked = (buf[1] >> 7) & 1;

    uint64_t payload_len = buf[1] & 0x7F;

    size_t offset = 2;

    if (payload_len == 126)
    {
        if (conn->buffer_length < 4)
        {
            return BB_ERROR(BB_ERR_INTERNAL, "Incomplete frame");
        }

        payload_len =
            ((uint16_t)buf[2] << 8) |
            ((uint16_t)buf[3]);

        offset += 2;
    }

    frame->payload_length = payload_len;

    if (frame->masked)
    {
        if (conn->buffer_length < offset + 4)
        {
            return BB_ERROR(BB_ERR_INTERNAL, "Incomplete frame");
        }

        memcpy(frame->masking_key, buf + offset, 4);

        offset += 4;
    }

    if (conn->buffer_length < offset + payload_len)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Incomplete frame");
    }

    frame->payload = malloc(payload_len + 1);

    if (!frame->payload)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
    }

    memcpy(frame->payload, buf + offset, payload_len);

    if (frame->masked)
    {
        for (uint64_t i = 0; i < payload_len; ++i)
        {
            frame->payload[i] ^=
                frame->masking_key[i % 4];
        }
    }

    frame->payload[payload_len] = '\0';

    /*
     * Consume frame bytes from connection buffer.
     */
    size_t consumed = offset + payload_len;

    size_t remaining = conn->buffer_length - consumed;

    if (remaining > 0)
    {
        memmove(conn->buffer, conn->buffer + consumed, remaining);
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

    conn->write_buffer = (char *)buffer;
    conn->write_length = pos;
    conn->write_offset = 0;

    return BB_SUCCESS();
}

bb_error_t bb_websocket_send_text(bb_websocket_t *ws, const char *text)
{
    bb_ws_frame_t frame = {0};

    frame.fin = 1;
    frame.opcode = BB_WS_TEXT;

    frame.payload = (char *)text;
    frame.payload_length = strlen(text);

    return bb_websocket_queue_frame(ws, &frame);
}

bb_error_t bb_websocket_send_binary(bb_websocket_t *ws, const void *data, size_t length)
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

static bb_error_t _bb_websocket_send_control(bb_websocket_t *ws, bb_ws_opcode_t opcode, const void *payload, size_t length)
{
    bb_ws_frame_t frame = {0};

    frame.fin = 1;
    frame.opcode = opcode;

    frame.payload = (char *)payload;
    frame.payload_length = length;

    return bb_websocket_queue_frame(ws, &frame);
}

bb_error_t bb_websocket_send_ping(bb_websocket_t *ws)
{
    return _bb_websocket_send_control(ws, BB_WS_PING, NULL, 0);
}

bb_error_t bb_websocket_send_pong(bb_websocket_t *ws)
{
    return _bb_websocket_send_control(ws, BB_WS_PONG, NULL, 0);
}

bb_error_t bb_websocket_send_close(bb_websocket_t *ws)
{
    return _bb_websocket_send_control(ws, BB_WS_CLOSE, NULL, 0);
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

    bb_error_t err = bb_websocket_read_frame(session->websocket, &frame);

    if (err.code == BB_ERR_INTERNAL)
    {
        return (bb_read_status_t){ BB_READ_MORE, BB_SUCCESS() };
    }

    if (BB_FAILED(err))
    {
        return (bb_read_status_t){ BB_READ_ERROR, err };
    }

    bb_ws_message_t msg;

    err = bb_ws_frame_to_message(&frame, &msg);

    if (BB_FAILED(err))
    {
        bb_ws_frame_destroy(&frame);
        return (bb_read_status_t){ BB_READ_ERROR, err };
    }

    session->handler(&session->context, &msg);

    bb_ws_frame_destroy(&frame);

    if (session->connection->write_buffer)
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
