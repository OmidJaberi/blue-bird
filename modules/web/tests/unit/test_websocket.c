#include "blue-bird/web/websocket/websocket.h"
#include "websocket/websocket_internal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bb_connection_t *create_test_connection(void)
{
    bb_connection_t *conn = calloc(1, sizeof(*conn));

    assert(conn);

    conn->fd = -1;

    return conn;
}

void test_websocket_create_destroy(void)
{
    printf("\tTesting websocket create/destroy...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    assert(ws != NULL);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_queue_text_frame(void)
{
    printf("\tTesting text frame serialization...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_text(ws, "hello");

    assert(err.code == BB_OK);

    assert(conn->write_data != NULL);

    unsigned char *buf = (unsigned char *)conn->write_data->write_buffer;

    assert(buf[0] == 0x81);

    assert(buf[1] == 5);

    assert(memcmp(buf + 2, "hello", 5) == 0);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_queue_binary_frame(void)
{
    printf("\tTesting binary frame serialization...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    unsigned char payload[] =
    {
        0x01,
        0x02,
        0x03,
        0x04
    };

    bb_error_t err = bb_websocket_queue_binary(ws, payload, sizeof(payload));

    assert(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)conn->write_data->write_buffer;

    assert(buf[0] == 0x82);

    assert(buf[1] == 4);

    assert(memcmp(buf + 2, payload, 4) == 0);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_queue_ping(void)
{
    printf("\tTesting ping frame...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_ping(ws);

    assert(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)conn->write_data->write_buffer;

    assert(buf[0] == 0x89);

    assert(buf[1] == 0);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_queue_pong(void)
{
    printf("\tTesting pong frame...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_pong(ws);

    assert(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)conn->write_data->write_buffer;

    assert(buf[0] == 0x8A);

    assert(buf[1] == 0);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_queue_close(void)
{
    printf("\tTesting close frame...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_error_t err = bb_websocket_queue_close(ws);

    assert(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)conn->write_data->write_buffer;

    assert(buf[0] == 0x88);

    assert(buf[1] == 0);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_parse_unmasked_text_frame(void)
{
    printf("\tTesting frame parsing...\n");

    bb_connection_t *conn = create_test_connection();

    conn->buffer = malloc(7);

    memcpy(conn->buffer, "\x81\x05hello", 7);

    conn->buffer_length = 7;
    conn->buffer_capacity = 7;

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(ws, &frame);

    assert(err.code == BB_OK);

    assert(frame.fin == 1);

    assert(frame.opcode == BB_WS_TEXT);

    assert(frame.payload_length == 5);

    assert(strcmp(frame.payload, "hello") == 0);

    bb_ws_frame_destroy(&frame);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_parse_masked_text_frame(void)
{
    printf("\tTesting masked frame parsing...\n");

    bb_connection_t *conn = create_test_connection();

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

    conn->buffer = malloc(sizeof(frame_bytes));

    memcpy(conn->buffer, frame_bytes, sizeof(frame_bytes));

    conn->buffer_length = sizeof(frame_bytes);

    conn->buffer_capacity = sizeof(frame_bytes);

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(ws, &frame);

    assert(err.code == BB_OK);

    assert(frame.opcode == BB_WS_TEXT);

    assert(strcmp(frame.payload, "Hello") == 0);

    bb_ws_frame_destroy(&frame);

    bb_websocket_destroy(ws);

    bb_connection_destroy(conn);
}

void test_invalid_arguments(void)
{
    printf("\tTesting invalid arguments...\n");

    bb_error_t err;

    err = bb_websocket_queue_text(NULL, "hello");

    assert(err.code != BB_OK);

    err = bb_websocket_queue_binary(NULL, "abc", 3);

    assert(err.code != BB_OK);
}

void test_websocket_accept_key(void)
{
    printf("\tTesting websocket accept key...\n");

    char *accept = bb_websocket_accept_key("dGhlIHNhbXBsZSBub25jZQ==");

    assert(accept != NULL);

    assert(strcmp(accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);

    free(accept);
}

void test_parse_multiple_frames(void)
{
    printf("\tTesting multiple frames...\n");

    bb_connection_t *conn = create_test_connection();

    unsigned char frames[] =
    {
        0x81, 0x05,
        'h','e','l','l','o',

        0x81, 0x05,
        'w','o','r','l','d'
    };

    conn->buffer = malloc(sizeof(frames));

    memcpy(conn->buffer, frames, sizeof(frames));

    conn->buffer_length = sizeof(frames);
    conn->buffer_capacity = sizeof(frames);

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_ws_frame_t frame = {0};

    bb_error_t err = bb_websocket_read_frames(ws, &frame);

    assert(err.code == BB_OK);

    assert(strcmp(frame.payload, "hello") == 0);
    assert(frame.next != NULL);

    assert(strcmp(frame.next->payload, "world") == 0);
    assert(frame.next->next == NULL);

    assert(conn->buffer_length == 0);

    bb_ws_frame_destroy(&frame);

    bb_websocket_destroy(ws);
    bb_connection_destroy(conn);
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

    printf("All tests passed.\n");

    return 0;
}
