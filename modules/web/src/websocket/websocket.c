#include "blue-bird/web/websocket/websocket.h"
#include "blue-bird/error/error.h"

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

bb_websocket_t *bb_websocket_create(bb_connection_t *connection)
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

    return ws;
}

void bb_websocket_destroy(bb_websocket_t *ws)
{
    free(ws);
}

void bb_ws_frame_destroy(bb_ws_frame_t *frame)
{
    if (!frame)
    {
        return;
    }

    free(frame->payload);

    memset(frame, 0, sizeof(*frame));
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

    return BB_SUCCESS();
}

bb_error_t bb_websocket_write_frame(bb_websocket_t *ws, const bb_ws_frame_t *frame)
{
    if (!ws || !frame)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    bb_connection_t *conn = ws->connection;

    size_t size = 2 + (frame->payload_length >= 126 ? 2 : 0) + frame->payload_length;

    char *buffer = malloc(size);

    if (!buffer)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
    }

    uint8_t *out = (uint8_t *)buffer;

    size_t pos = 0;

    out[pos++] = 0x80 | (frame->opcode & 0x0F);

    if (frame->payload_length < 126)
    {
        out[pos++] = frame->payload_length;
    }
    else
    {
        out[pos++] = 126;

        out[pos++] = (frame->payload_length >> 8) & 0xFF;

        out[pos++] = frame->payload_length & 0xFF;
    }

    memcpy(out + pos, frame->payload, frame->payload_length);

    conn->write_buffer = buffer;
    conn->write_length = size;
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

    return bb_websocket_write_frame(ws, &frame);
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

    return bb_websocket_write_frame(ws, &frame);
}

static bb_error_t _bb_websocket_send_control(bb_websocket_t *ws, bb_ws_opcode_t opcode, const void *payload, size_t length)
{
    bb_ws_frame_t frame = {0};

    frame.fin = 1;
    frame.opcode = opcode;

    frame.payload = (char *)payload;
    frame.payload_length = length;

    return bb_websocket_write_frame(ws, &frame);
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
