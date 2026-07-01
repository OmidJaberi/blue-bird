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

    bb_server_add_websocket(server, "/ws", echo_handler);

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
 * Client callbacks
 * ============================================================ */

static void connect_cb(bb_ws_client_t *client, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    err = bb_ws_client_send_text(client, "hello");

    assert(!BB_FAILED(err));
}

bb_error_t message_cb(bb_ws_context_t *ctx, const bb_ws_message_t *msg)
{
    (void) ctx;

    assert(msg->type == BB_WS_MESSAGE_TEXT);
    assert(msg->length == 5);
    assert(memcmp(msg->data, "hello", 5) == 0);

    finished = 1;
    return BB_SUCCESS();
}

/* ============================================================
 * Tests
 * ============================================================ */

static void websocket_echo_test(void)
{
    printf("\tTesting WebSocket echo...\n");

    finished = 0;

    pthread_t thread;

    assert(pthread_create(&thread, NULL, server_thread, NULL) == 0);

    bb_runtime_t *runtime = bb_runtime_create();

    bb_ws_client_t *client = bb_ws_client_create_on_runtime(runtime);

    bb_ws_client_set_message_callback(client, message_cb, NULL);

    bb_ws_client_connect_async(client, "ws://127.0.0.1:8080/ws", connect_cb, NULL);

    while (!finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_ws_client_destroy(client);
    bb_runtime_destroy(runtime);

    pthread_join(thread, NULL);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    printf("Testing websocket client/server integration:\n");

    websocket_echo_test();

    printf("All websocket integration tests passed.\n");

    return 0;
}

