#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"
#include "blue-bird/web/websocket/client.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>

#define TEST_PORT 8080

static volatile int finished = 0;

/* ============================================================
 * Server
 * ============================================================ */

static bb_error_t echo_handler(bb_ws_context_t *ctx, const bb_ws_message_t *msg)
{
    if (msg->type == BB_WS_MESSAGE_TEXT)
    {
        return bb_ws_send_text(ctx, msg->data);
    }
    if (msg->type == BB_WS_MESSAGE_BINARY)
    {
        return bb_ws_send_binary(ctx, msg->data, msg->length);
    }

    return BB_SUCCESS();
}

static void *server_thread(void *arg)
{
    (void)arg;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_server_t *server = bb_server_create_on_runtime(runtime, TEST_PORT);

    bb_server_add_websocket(server, "/echo", echo_handler);

    bb_server_start(server);

    while (!finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_server_destroy(server);
    bb_runtime_destroy(runtime);

    return NULL;
}

/* ============================================================
 * Tests
 * ============================================================ */

 // Echo test

static volatile int echo_finished = 0;

static void _echo_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    err = bb_websocket_send_text(ws, "hello");

    assert(!BB_FAILED(err));
}

bb_error_t _echo_message_cb(bb_ws_context_t *ctx, const bb_ws_message_t *msg)
{
    (void) ctx;

    assert(msg->type == BB_WS_MESSAGE_TEXT);
    assert(msg->length == 5);
    assert(memcmp(msg->data, "hello", 5) == 0);

    echo_finished = 1;
    return BB_SUCCESS();
}

static void websocket_echo_test(void)
{
    printf("\tTesting WebSocket echo...\n");

    echo_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_ws_client_t *client = bb_ws_client_create_on_runtime(runtime);

    bb_ws_client_set_message_callback(client, _echo_message_cb, NULL);

    bb_ws_client_connect_async(client, "ws://127.0.0.1:8080/echo", _echo_connect_cb, NULL);

    while (!echo_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_ws_client_destroy(client);
    bb_runtime_destroy(runtime);
}

 // Multi Message test

static volatile int messages = 0;

static void _multi_messages_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void) err;
    (void) userdata;

    bb_websocket_send_text(ws, "one");
    bb_websocket_send_text(ws, "two");
    bb_websocket_send_text(ws, "three");
}

bb_error_t _multi_messages_message_cb(bb_ws_context_t *ctx, const bb_ws_message_t *msg)
{
    (void) ctx;

    messages++;

    // printf("Message %d: %s\n", messages, (char *)msg->data);

    switch(messages)
    {
        case 1:
            assert(strcmp(msg->data, "one") == 0);
            break;

        case 2:
            assert(strcmp(msg->data, "two") == 0);
            break;

        case 3:
            assert(strcmp(msg->data, "three") == 0);
            break;
    }

    return BB_SUCCESS();
}

static void websocket_multi_message_test(void)
{
    printf("\tTesting WebSocket Multiple Messages...\n");

    echo_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_ws_client_t *client = bb_ws_client_create_on_runtime(runtime);

    bb_ws_client_set_message_callback(client, _multi_messages_message_cb, NULL);

    bb_ws_client_connect_async(client, "ws://127.0.0.1:8080/echo", _multi_messages_connect_cb, NULL);

    while (messages < 3)
    {
        bb_runtime_tick(runtime);
    }

    bb_ws_client_destroy(client);
    bb_runtime_destroy(runtime);
}

// Large Message test

#define LARGE_MESSAGE_SIZE 8192

static volatile int large_finished = 0;

static char large_message[LARGE_MESSAGE_SIZE + 1];

static void _large_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    bb_error_t send_err = bb_websocket_send_text(ws, large_message);

    assert(!BB_FAILED(send_err));
}

bb_error_t _large_message_cb(bb_ws_context_t *ctx, const bb_ws_message_t *msg)
{
    (void)ctx;

    assert(msg->type == BB_WS_MESSAGE_TEXT);
    assert(msg->length == LARGE_MESSAGE_SIZE);
    assert(memcmp(msg->data, large_message, LARGE_MESSAGE_SIZE) == 0);

    large_finished = 1;

    return BB_SUCCESS();
}

static void websocket_large_message_test(void)
{
    printf("\tTesting WebSocket Large Message...\n");

    large_finished = 0;

    memset(large_message, 'A', LARGE_MESSAGE_SIZE);
    large_message[LARGE_MESSAGE_SIZE] = '\0';

    bb_runtime_t *runtime = bb_runtime_create();

    bb_ws_client_t *client = bb_ws_client_create_on_runtime(runtime);

    bb_ws_client_set_message_callback(client, _large_message_cb, NULL);

    bb_ws_client_connect_async(client, "ws://127.0.0.1:8080/echo", _large_connect_cb, NULL);

    while (!large_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_ws_client_destroy(client);
    bb_runtime_destroy(runtime);
}

// Binary Message test

#define BINARY_MESSAGE_SIZE 7

static volatile int binary_finished = 0;

static uint8_t binary_message[BINARY_MESSAGE_SIZE] = {
    0x00,
    0x01,
    0x02,
    0x03,
    0xFF,
    0x7A,
    0x10
};

static void _binary_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    bb_error_t send_err = bb_websocket_send_binary(ws, binary_message, BINARY_MESSAGE_SIZE);

    assert(!BB_FAILED(send_err));
}

bb_error_t _binary_message_cb(bb_ws_context_t *ctx, const bb_ws_message_t *msg)
{
    (void)ctx;

    assert(msg->type == BB_WS_MESSAGE_BINARY);
    assert(msg->length == BINARY_MESSAGE_SIZE);
    assert(memcmp(msg->data, binary_message, BINARY_MESSAGE_SIZE) == 0);

    binary_finished = 1;

    return BB_SUCCESS();
}

static void websocket_binary_message_test(void)
{
    printf("\tTesting WebSocket Binary Message...\n");

    binary_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_ws_client_t *client = bb_ws_client_create_on_runtime(runtime);

    bb_ws_client_set_message_callback(client, _binary_message_cb, NULL);

    bb_ws_client_connect_async(
        client,
        "ws://127.0.0.1:8080/echo",
        _binary_connect_cb,
        NULL
    );

    while (!binary_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_ws_client_destroy(client);
    bb_runtime_destroy(runtime);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    pthread_t thread;

    assert(pthread_create(&thread, NULL, server_thread, NULL) == 0);

    printf("Testing websocket client/server integration:\n");

    websocket_echo_test();
    websocket_multi_message_test();
    websocket_large_message_test();
    websocket_binary_message_test();

    finished = 1;
    printf("All websocket integration tests passed.\n");

    pthread_join(thread, NULL);
    return 0;
}

