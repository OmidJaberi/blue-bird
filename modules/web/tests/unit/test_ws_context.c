#include "blue-bird/web/websocket/context.h"

#include "websocket/context_internal.h"
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

static void destroy_test_connection(bb_connection_t *conn)
{
    if (!conn)
    {
        return;
    }

    free(conn->buffer);

    free(conn->write_buffer);

    free(conn);
}

void test_context_create_destroy(void)
{
    printf("\tTesting context create/destroy...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    assert(ws != NULL);

    bb_ws_context_t *ctx = bb_ws_context_create(ws);

    assert(ctx != NULL);

    bb_ws_context_destroy(ctx);

    bb_websocket_destroy(ws);

    destroy_test_connection(conn);
}

void test_context_userdata(void)
{
    printf("\tTesting context userdata...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_ws_context_t *ctx = bb_ws_context_create(ws);

    int value = 42;

    bb_ws_set_userdata(ctx, &value);

    assert(bb_ws_userdata(ctx) == &value);

    bb_ws_context_destroy(ctx);

    bb_websocket_destroy(ws);

    destroy_test_connection(conn);
}

void test_send_text(void)
{
    printf("\tTesting context send text...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_ws_context_t *ctx = bb_ws_context_create(ws);

    bb_error_t err = bb_ws_send_text(ctx, "hello");

    assert(err.code == BB_OK);

    assert(conn->write_buffer != NULL);

    unsigned char *buf = (unsigned char *)conn->write_buffer;

    assert(buf[0] == 0x81);

    assert(buf[1] == 5);

    assert(memcmp(buf + 2, "hello", 5) == 0);

    bb_ws_context_destroy(ctx);

    bb_websocket_destroy(ws);

    destroy_test_connection(conn);
}

void test_send_binary(void)
{
    printf("\tTesting context send binary...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_ws_context_t *ctx = bb_ws_context_create(ws);

    unsigned char payload[] =
    {
        1,
        2,
        3,
        4
    };

    bb_error_t err = bb_ws_send_binary(ctx, payload, sizeof(payload));

    assert(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)conn->write_buffer;

    assert(buf[0] == 0x82);

    assert(buf[1] == 4);

    assert(memcmp(buf + 2, payload, sizeof(payload)) == 0);

    bb_ws_context_destroy(ctx);

    bb_websocket_destroy(ws);

    destroy_test_connection(conn);
}

void test_close(void)
{
    printf("\tTesting context close...\n");

    bb_connection_t *conn = create_test_connection();

    bb_websocket_t *ws = bb_websocket_create(conn, BB_WEBSOCKET_SERVER);

    bb_ws_context_t *ctx = bb_ws_context_create(ws);

    bb_error_t err = bb_ws_close(ctx);

    assert(err.code == BB_OK);

    unsigned char *buf = (unsigned char *)conn->write_buffer;

    assert(buf[0] == 0x88);

    assert(buf[1] == 0);

    bb_ws_context_destroy(ctx);

    bb_websocket_destroy(ws);

    destroy_test_connection(conn);
}

int main(void)
{
    printf("Running WebSocket Context tests...\n");

    test_context_create_destroy();

    test_context_userdata();

    test_send_text();

    test_send_binary();

    test_close();

    printf("All tests passed.\n");

    return 0;
}
