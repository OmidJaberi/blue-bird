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

static void _echo_connect_cb(bb_ws_client_t *client, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    err = bb_ws_client_send_text(client, "hello");

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

static void _multi_messages_connect_cb(bb_ws_client_t *client, bb_error_t err, void *userdata)
{
    (void) err;
    (void) userdata;

    bb_ws_client_send_text(client, "one");
    bb_ws_client_send_text(client, "two");
    bb_ws_client_send_text(client, "three");
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
            finished = 1;
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

    finished = 1;
    printf("All websocket integration tests passed.\n");

    pthread_join(thread, NULL);
    return 0;
}

