#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"
#include "blue-bird/web/websocket/websocket.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>

#define TEST_PORT 8080

static volatile int finished = 0;

/* ============================================================
 * Server
 * ============================================================ */

static bb_error_t echo_handler(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    if (msg->type == BB_WS_MESSAGE_TEXT)
    {
        return bb_websocket_send_text(ws, msg->data);
    }
    if (msg->type == BB_WS_MESSAGE_BINARY)
    {
        return bb_websocket_send_binary(ws, msg->data, msg->length);
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

bb_error_t _echo_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void) ws;

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

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _echo_message_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8080/echo", _echo_connect_cb, NULL);

    while (!echo_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
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

bb_error_t _multi_messages_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void) ws;

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

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _multi_messages_message_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8080/echo", _multi_messages_connect_cb, NULL);

    while (messages < 3)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
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

bb_error_t _large_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

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

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _large_message_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8080/echo", _large_connect_cb, NULL);

    while (!large_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
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

bb_error_t _binary_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

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

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _binary_message_cb, NULL);

    bb_websocket_connect(
        client,
        "ws://127.0.0.1:8080/echo",
        _binary_connect_cb,
        NULL
    );

    while (!binary_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
}

// Large Binary Message test

#define LARGE_BINARY_SIZE 8192

static volatile int large_binary_finished = 0;

static uint8_t large_binary[LARGE_BINARY_SIZE];

static void _large_binary_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    bb_error_t send_err =
        bb_websocket_send_binary(ws, large_binary, LARGE_BINARY_SIZE);

    assert(!BB_FAILED(send_err));
}

bb_error_t _large_binary_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    assert(msg->type == BB_WS_MESSAGE_BINARY);
    assert(msg->length == LARGE_BINARY_SIZE);
    assert(memcmp(msg->data, large_binary, LARGE_BINARY_SIZE) == 0);

    large_binary_finished = 1;

    return BB_SUCCESS();
}

static void websocket_large_binary_test(void)
{
    printf("\tTesting WebSocket Large Binary Message...\n");

    large_binary_finished = 0;

    for (size_t i = 0; i < LARGE_BINARY_SIZE; i++)
    {
        large_binary[i] = (uint8_t)(i & 0xFF);
    }

    bb_runtime_t *runtime = bb_runtime_create();

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _large_binary_message_cb, NULL);

    bb_websocket_connect(
        client,
        "ws://127.0.0.1:8080/echo",
        _large_binary_connect_cb,
        NULL
    );

    while (!large_binary_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
}

// Sequential Connections test

#define SEQUENTIAL_CONNECTIONS 100

static volatile int sequential_finished = 0;

static void _sequential_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    err = bb_websocket_send_text(ws, "ping");

    assert(!BB_FAILED(err));
}

bb_error_t _sequential_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    assert(msg->type == BB_WS_MESSAGE_TEXT);
    assert(msg->length == 4);
    assert(memcmp(msg->data, "ping", 4) == 0);

    sequential_finished = 1;

    return BB_SUCCESS();
}

static void websocket_sequential_connections_test(void)
{
    printf("\tTesting WebSocket Sequential Connections...\n");

    bb_runtime_t *runtime = bb_runtime_create();

    for (int i = 0; i < SEQUENTIAL_CONNECTIONS; i++)
    {
        sequential_finished = 0;

        bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

        bb_websocket_set_message_callback(client, _sequential_message_cb, NULL);

        bb_websocket_connect(client, "ws://127.0.0.1:8080/echo", _sequential_connect_cb, NULL);

        while (!sequential_finished)
        {
            bb_runtime_tick(runtime);
        }

        bb_websocket_destroy(client);
    }

    bb_runtime_destroy(runtime);
}

// Multiple Clients test

#define CLIENT_COUNT 5

static volatile int clients_finished = 0;

static const char *client_messages[CLIENT_COUNT] = {
    "client0",
    "client1",
    "client2",
    "client3",
    "client4"
};

static void _clients_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    assert(!BB_FAILED(err));

    const char *text = userdata;

    err = bb_websocket_send_text(ws, text);

    assert(!BB_FAILED(err));
}

bb_error_t _clients_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    for (int i = 0; i < CLIENT_COUNT; i++)
    {
        if (strcmp(msg->data, client_messages[i]) == 0)
        {
            clients_finished++;
            return BB_SUCCESS();
        }
    }

    assert(0 && "Unexpected message");

    return BB_SUCCESS();
}

static void websocket_multiple_clients_test(void)
{
    printf("\tTesting WebSocket Multiple Clients...\n");

    clients_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_websocket_t *clients[CLIENT_COUNT];

    for (int i = 0; i < CLIENT_COUNT; i++)
    {
        clients[i] = bb_websocket_create_on_runtime(runtime);

        bb_websocket_set_message_callback(clients[i], _clients_message_cb, NULL);

        bb_websocket_connect(clients[i], "ws://127.0.0.1:8080/echo", _clients_connect_cb, (void *)client_messages[i]);
    }

    while (clients_finished < CLIENT_COUNT)
    {
        bb_runtime_tick(runtime);
    }

    for (int i = 0; i < CLIENT_COUNT; i++)
    {
        bb_websocket_destroy(clients[i]);
    }

    bb_runtime_destroy(runtime);
}

// Many Messages test

#define MANY_MESSAGES 10000

static volatile int many_messages_received = 0;

static void _many_messages_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    for (int i = 0; i < MANY_MESSAGES; i++)
    {
        bb_error_t send_err = bb_websocket_send_text(ws, "ping");
        assert(!BB_FAILED(send_err));
    }
}

bb_error_t _many_messages_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    assert(msg->type == BB_WS_MESSAGE_TEXT);
    assert(msg->length == 4);
    assert(memcmp(msg->data, "ping", 4) == 0);

    many_messages_received++;

    return BB_SUCCESS();
}

static void websocket_many_messages_test(void)
{
    printf("\tTesting WebSocket Many Messages...\n");

    many_messages_received = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _many_messages_message_cb, NULL);

    bb_websocket_connect(
        client,
        "ws://127.0.0.1:8080/echo",
        _many_messages_connect_cb,
        NULL
    );

    while (many_messages_received < MANY_MESSAGES)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
}

// Ping Pong test

static volatile int pong_received = 0;

static void _pong_cb(bb_websocket_t *ws, const void *payload, size_t length, void *userdata)
{
    (void)ws;
    (void)userdata;

    assert(length == 4);
    assert(memcmp(payload, "ping", 4) == 0);

    pong_received = 1;
}

static void _ping_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    assert(!BB_FAILED(err));

    bb_error_t e = bb_websocket_send_ping(ws, "ping", 4);
    assert(!BB_FAILED(e));
}

static void websocket_ping_pong_test(void)
{
    printf("\tTesting WebSocket Ping/Pong...\n");

    pong_received = 0;

    bb_runtime_t *runtime = bb_runtime_create();

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_pong_callback(client, _pong_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8080/echo", _ping_connect_cb, NULL);

    while (!pong_received)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
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
    websocket_large_binary_test();
    websocket_sequential_connections_test();
    websocket_multiple_clients_test();
    websocket_many_messages_test();
    websocket_ping_pong_test();

    finished = 1;
    printf("All websocket integration tests passed.\n");

    pthread_join(thread, NULL);
    return 0;
}
