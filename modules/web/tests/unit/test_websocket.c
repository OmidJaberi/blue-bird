#include "blue-bird/web/websocket/websocket.h"
#include "websocket/websocket_internal.h"

#include <blue-bird/utils/platform.h>
#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bb_async_connection_t *create_test_connection(void)
{
    bb_async_connection_t *async_conn = calloc(1, sizeof(*async_conn));

    BB_ASSERT(async_conn);

    async_conn->connection = calloc(1, sizeof(*async_conn->connection));

    BB_ASSERT(async_conn->connection);

    async_conn->connection->fd = -1;

    return async_conn;
}

void test_websocket_create_destroy(void)
{
    printf("\tTesting websocket create/destroy...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    BB_ASSERT(ws != NULL);

    bb_websocket_destroy(ws);
}

void test_queue_text_frame(void)
{
    printf("\tTesting text frame serialization...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_text(ws, "hello");

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(async_conn->connection->write_data != NULL);

    unsigned char *buf = (unsigned char *)async_conn->connection->write_data->write_buffer;

    BB_ASSERT(buf[0] == 0x81);

    BB_ASSERT(buf[1] == 5);

    BB_ASSERT(memcmp(buf + 2, "hello", 5) == 0);

    bb_websocket_destroy(ws);
}

void test_queue_binary_frame(void)
{
    printf("\tTesting binary frame serialization...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    unsigned char payload[] =
    {
        0x01,
        0x02,
        0x03,
        0x04
    };

    bb_error_t err = bb_websocket_queue_binary(ws, payload, sizeof(payload));

    BB_ASSERT(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)async_conn->connection->write_data->write_buffer;

    BB_ASSERT(buf[0] == 0x82);

    BB_ASSERT(buf[1] == 4);

    BB_ASSERT(memcmp(buf + 2, payload, 4) == 0);

    bb_websocket_destroy(ws);
}

void test_queue_ping(void)
{
    printf("\tTesting ping frame...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_ping(ws, NULL, 0);

    BB_ASSERT(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)async_conn->connection->write_data->write_buffer;

    BB_ASSERT(buf[0] == 0x89);

    BB_ASSERT(buf[1] == 0);

    bb_websocket_destroy(ws);
}

void test_queue_pong(void)
{
    printf("\tTesting pong frame...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_pong(ws, NULL, 0);

    BB_ASSERT(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)async_conn->connection->write_data->write_buffer;

    BB_ASSERT(buf[0] == 0x8A);

    BB_ASSERT(buf[1] == 0);

    bb_websocket_destroy(ws);
}

void test_queue_close(void)
{
    printf("\tTesting close frame...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_close(ws, 1000, NULL);

    BB_ASSERT(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)async_conn->connection->write_data->write_buffer;

    BB_ASSERT(buf[0] == 0x88);

    BB_ASSERT(buf[1] == 2);

    uint16_t code;
    memcpy(&code, buf + 2, sizeof(code));
    BB_ASSERT(ntohs(code) == 1000);

    bb_websocket_destroy(ws);
}

void test_parse_unmasked_text_frame(void)
{
    printf("\tTesting frame parsing...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    async_conn->connection->buffer = malloc(7);

    memcpy(async_conn->connection->buffer, "\x81\x05hello", 7);

    async_conn->connection->buffer_length = 7;
    async_conn->connection->buffer_capacity = 7;

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(ws, &frame);

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(frame.fin == 1);

    BB_ASSERT(frame.opcode == BB_WS_TEXT);

    BB_ASSERT(frame.payload_length == 5);

    BB_ASSERT(strcmp(frame.payload, "hello") == 0);

    bb_ws_frame_destroy(&frame);

    bb_websocket_destroy(ws);
}

void test_parse_masked_text_frame(void)
{
    printf("\tTesting masked frame parsing...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    unsigned char frame_bytes[] =
    {
        0x81,
        0x85,

        0x37,
        0xFA,
        0x21,
        0x3D,

        0x7F,
        0x9F,
        0x4D,
        0x51,
        0x58
    };

    async_conn->connection->buffer = malloc(sizeof(frame_bytes));

    memcpy(async_conn->connection->buffer, frame_bytes, sizeof(frame_bytes));

    async_conn->connection->buffer_length = sizeof(frame_bytes);

    async_conn->connection->buffer_capacity = sizeof(frame_bytes);

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(ws, &frame);

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(frame.opcode == BB_WS_TEXT);

    BB_ASSERT(strcmp(frame.payload, "Hello") == 0);

    bb_ws_frame_destroy(&frame);

    bb_websocket_destroy(ws);
}

void test_invalid_arguments(void)
{
    printf("\tTesting invalid arguments...\n");

    bb_error_t err;

    err = bb_websocket_queue_text(NULL, "hello");

    BB_ASSERT(err.code != BB_OK);

    err = bb_websocket_queue_binary(NULL, "abc", 3);

    BB_ASSERT(err.code != BB_OK);
}

void test_websocket_accept_key(void)
{
    printf("\tTesting websocket accept key...\n");

    char *accept = bb_websocket_accept_key("dGhlIHNhbXBsZSBub25jZQ==");

    BB_ASSERT(accept != NULL);

    BB_ASSERT(strcmp(accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);

    free(accept);
}

void test_parse_multiple_frames(void)
{
    printf("\tTesting multiple frames...\n");

    bb_async_connection_t *async_conn = create_test_connection();

    unsigned char frames[] =
    {
        0x81, 0x05,
        'h','e','l','l','o',

        0x81, 0x05,
        'w','o','r','l','d'
    };

    async_conn->connection->buffer = malloc(sizeof(frames));

    memcpy(async_conn->connection->buffer, frames, sizeof(frames));

    async_conn->connection->buffer_length = sizeof(frames);
    async_conn->connection->buffer_capacity = sizeof(frames);

    bb_websocket_t *ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(ws, &frame);

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(strcmp(frame.payload, "hello") == 0);
    BB_ASSERT(frame.next != NULL);

    BB_ASSERT(strcmp(frame.next->payload, "world") == 0);
    BB_ASSERT(frame.next->next == NULL);

    BB_ASSERT(async_conn->connection->buffer_length == 0);

    bb_ws_frame_destroy(&frame);

    bb_websocket_destroy(ws);
}

int main(void)
{
    printf("Running WebSocket tests...\n");

    test_websocket_create_destroy();

    test_queue_text_frame();

    test_queue_binary_frame();

    test_queue_ping();

    test_queue_pong();

    test_queue_close();

    test_parse_unmasked_text_frame();

    test_parse_masked_text_frame();

    test_invalid_arguments();

    test_websocket_accept_key();

    test_parse_multiple_frames();

    bb_runtime_destroy(bb_runtime_default());
    printf("All tests passed.\n");

    return 0;
}
