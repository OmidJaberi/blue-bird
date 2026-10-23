#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"
#include "blue-bird/web/websocket/websocket.h"
#include "blue-bird/utils/platform.h"
#include "server_internal.h"
#include "websocket/websocket_internal.h"

#include <blue-bird/error/assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TEST_PORT 8081

/* ============================================================
 * Server
 * ============================================================ */

 bb_runtime_t *server_runtime = NULL;
 bb_server_t *server = NULL;

static bb_error_t echo_handler(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    if (bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT)
    {
        return bb_websocket_send_text(ws, bb_ws_message_get_data(msg));
    }
    if (bb_ws_message_get_type(msg) == BB_WS_MESSAGE_BINARY)
    {
        return bb_websocket_send_binary(ws, bb_ws_message_get_data(msg), bb_ws_message_get_length(msg));
    }

    return BB_SUCCESS();
}

static void *server_thread(void *arg)
{
    (void)arg;

    server_runtime = bb_runtime_create();

    server = bb_server_create_on_runtime(server_runtime, TEST_PORT);

    bb_server_add_websocket(server, "/echo", echo_handler);

    bb_server_start(server);

    bb_runtime_run(server_runtime);

    bb_server_destroy(server);
    bb_runtime_destroy(server_runtime);

    return NULL;
}

static void check_server(void)
{
    /*
     * The server processes disconnects on its own thread's event loop.
     * Give it a bounded amount of time to notice the client went away
     * and run auto-cleanup before failing the assertion — this isn't
     * synchronized with the test thread, so it's inherently async.
     */
    const int max_attempts = 5000;
    int attempts = 0;

    while (server->ws_list->head != NULL && attempts < max_attempts)
    {
        bb_usleep(1000); /* 1ms */
        attempts++;
    }

    BB_ASSERT(server->ws_list->head == NULL);
}

/* ============================================================
 * Tests
 * ============================================================ */

 // Echo test

static volatile int echo_finished = 0;

static void _echo_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    BB_ASSERT(!BB_FAILED(err));

    err = bb_websocket_send_text(ws, "hello");

    BB_ASSERT(!BB_FAILED(err));
}

bb_error_t _echo_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void) ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT);
    BB_ASSERT(bb_ws_message_get_length(msg) == 5);
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), "hello", 5) == 0);

    echo_finished = 1;
    return BB_SUCCESS();
}

static void websocket_echo_test(void)
{
    printf("\tTesting WebSocket echo...\n");

    echo_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _echo_message_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _echo_connect_cb, NULL);

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

    // printf("Message %d: %s\n", messages, (char *)bb_ws_message_get_data(msg));

    switch(messages)
    {
        case 1:
            BB_ASSERT(strcmp(bb_ws_message_get_data(msg), "one") == 0);
            break;

        case 2:
            BB_ASSERT(strcmp(bb_ws_message_get_data(msg), "two") == 0);
            break;

        case 3:
            BB_ASSERT(strcmp(bb_ws_message_get_data(msg), "three") == 0);
            break;
    }

    return BB_SUCCESS();
}

static void websocket_multi_message_test(void)
{
    printf("\tTesting WebSocket Multiple Messages...\n");

    echo_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _multi_messages_message_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _multi_messages_connect_cb, NULL);

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

    BB_ASSERT(!BB_FAILED(err));

    bb_error_t send_err = bb_websocket_send_text(ws, large_message);

    BB_ASSERT(!BB_FAILED(send_err));
}

bb_error_t _large_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT);
    BB_ASSERT(bb_ws_message_get_length(msg) == LARGE_MESSAGE_SIZE);
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), large_message, LARGE_MESSAGE_SIZE) == 0);

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
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _large_message_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _large_connect_cb, NULL);

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

    BB_ASSERT(!BB_FAILED(err));

    bb_error_t send_err = bb_websocket_send_binary(ws, binary_message, BINARY_MESSAGE_SIZE);

    BB_ASSERT(!BB_FAILED(send_err));
}

bb_error_t _binary_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == BB_WS_MESSAGE_BINARY);
    BB_ASSERT(bb_ws_message_get_length(msg) == BINARY_MESSAGE_SIZE);
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), binary_message, BINARY_MESSAGE_SIZE) == 0);

    binary_finished = 1;

    return BB_SUCCESS();
}

static void websocket_binary_message_test(void)
{
    printf("\tTesting WebSocket Binary Message...\n");

    binary_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _binary_message_cb, NULL);

    bb_websocket_connect(
        client,
        "ws://127.0.0.1:8081/echo",
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

    BB_ASSERT(!BB_FAILED(err));

    bb_error_t send_err =
        bb_websocket_send_binary(ws, large_binary, LARGE_BINARY_SIZE);

    BB_ASSERT(!BB_FAILED(send_err));
}

bb_error_t _large_binary_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == BB_WS_MESSAGE_BINARY);
    BB_ASSERT(bb_ws_message_get_length(msg) == LARGE_BINARY_SIZE);
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), large_binary, LARGE_BINARY_SIZE) == 0);

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
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _large_binary_message_cb, NULL);

    bb_websocket_connect(
        client,
        "ws://127.0.0.1:8081/echo",
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

    BB_ASSERT(!BB_FAILED(err));

    err = bb_websocket_send_text(ws, "ping");

    BB_ASSERT(!BB_FAILED(err));
}

bb_error_t _sequential_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT);
    BB_ASSERT(bb_ws_message_get_length(msg) == 4);
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), "ping", 4) == 0);

    sequential_finished = 1;

    return BB_SUCCESS();
}

static void websocket_sequential_connections_test(void)
{
    printf("\tTesting WebSocket Sequential Connections...\n");

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    for (int i = 0; i < SEQUENTIAL_CONNECTIONS; i++)
    {
        sequential_finished = 0;

        bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

        bb_websocket_set_message_callback(client, _sequential_message_cb, NULL);

        bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _sequential_connect_cb, NULL);

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
    BB_ASSERT(!BB_FAILED(err));

    const char *text = userdata;

    err = bb_websocket_send_text(ws, text);

    BB_ASSERT(!BB_FAILED(err));
}

bb_error_t _clients_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    for (int i = 0; i < CLIENT_COUNT; i++)
    {
        if (strcmp(bb_ws_message_get_data(msg), client_messages[i]) == 0)
        {
            clients_finished++;
            return BB_SUCCESS();
        }
    }

    BB_ASSERT(0 && "Unexpected message");

    return BB_SUCCESS();
}

static void websocket_multiple_clients_test(void)
{
    printf("\tTesting WebSocket Multiple Clients...\n");

    clients_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *clients[CLIENT_COUNT];

    for (int i = 0; i < CLIENT_COUNT; i++)
    {
        clients[i] = bb_websocket_create_on_runtime(runtime);

        bb_websocket_set_message_callback(clients[i], _clients_message_cb, NULL);

        bb_websocket_connect(clients[i], "ws://127.0.0.1:8081/echo", _clients_connect_cb, (void *)client_messages[i]);
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

    BB_ASSERT(!BB_FAILED(err));

    for (int i = 0; i < MANY_MESSAGES; i++)
    {
        bb_error_t send_err = bb_websocket_send_text(ws, "ping");
        BB_ASSERT(!BB_FAILED(send_err));
    }
}

bb_error_t _many_messages_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT);
    BB_ASSERT(bb_ws_message_get_length(msg) == 4);
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), "ping", 4) == 0);

    many_messages_received++;

    return BB_SUCCESS();
}

static void websocket_many_messages_test(void)
{
    printf("\tTesting WebSocket Many Messages...\n");

    many_messages_received = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _many_messages_message_cb, NULL);

    bb_websocket_connect(
        client,
        "ws://127.0.0.1:8081/echo",
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

    BB_ASSERT(length == 4);
    BB_ASSERT(memcmp(payload, "ping", 4) == 0);

    pong_received = 1;
}

static void _ping_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    BB_ASSERT(!BB_FAILED(err));

    bb_error_t e = bb_websocket_send_ping(ws, "ping", 4);
    BB_ASSERT(!BB_FAILED(e));
}

static void websocket_ping_pong_test(void)
{
    printf("\tTesting WebSocket Ping/Pong...\n");

    pong_received = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_pong_callback(client, _pong_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _ping_connect_cb, NULL);

    while (!pong_received)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
}

// Close test

static volatile int close_done = 0;
static volatile int message_after_close = 0;

bb_error_t _close_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;
    (void)msg;

    if (close_done)
    {
        message_after_close++;
    }

    return BB_SUCCESS();
}

static void _close_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    BB_ASSERT(!BB_FAILED(err));

    bb_error_t e = bb_websocket_send_text(ws, "before-close");
    BB_ASSERT(!BB_FAILED(e));

    e = bb_websocket_send_close(ws, 1000, "normal");
    BB_ASSERT(!BB_FAILED(e));

    close_done = 1;
}

static void websocket_close_test(void)
{
    printf("\tTesting WebSocket Close...\n");

    close_done = 0;
    message_after_close = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_set_message_callback(client, _close_message_cb, NULL);

    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _close_connect_cb, NULL);

    while (!bb_runtime_is_empty(runtime))
    {
        bb_runtime_tick(runtime);
    }

    // after close, no new messages should arrive unexpectedly
    BB_ASSERT(message_after_close == 0);

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
}

// Dead Connection test

static volatile int dead_conn_connected = 0;

static void _dead_conn_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)ws;
    (void)userdata;

    BB_ASSERT(!BB_FAILED(err));

    dead_conn_connected = 1;
}

static void websocket_dead_connection_test(void)
{
    printf("\tTesting WebSocket Dead Connection Cleanup...\n");

    dead_conn_connected = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _dead_conn_connect_cb, NULL);

    while (!dead_conn_connected)
    {
        bb_runtime_tick(runtime);
    }

    /* Abrupt teardown: no send_close, no messages exchanged at all. */
    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);

    check_server();
}

// Multiple Dead Connections test

#define DEAD_CONNECTION_COUNT 10

static volatile int dead_conns_connected = 0;

static void _dead_conns_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)ws;
    (void)userdata;

    BB_ASSERT(!BB_FAILED(err));

    dead_conns_connected++;
}

static void websocket_multiple_dead_connections_test(void)
{
    printf("\tTesting WebSocket Multiple Dead Connections Cleanup...\n");

    dead_conns_connected = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *clients[DEAD_CONNECTION_COUNT];

    for (int i = 0; i < DEAD_CONNECTION_COUNT; i++)
    {
        clients[i] = bb_websocket_create_on_runtime(runtime);
        bb_websocket_connect(clients[i], "ws://127.0.0.1:8081/echo", _dead_conns_connect_cb, NULL);
    }

    while (dead_conns_connected < DEAD_CONNECTION_COUNT)
    {
        bb_runtime_tick(runtime);
    }

    /* Kill every connection abruptly, no close handshake on any of them. */
    for (int i = 0; i < DEAD_CONNECTION_COUNT; i++)
    {
        bb_websocket_destroy(clients[i]);
    }

    bb_runtime_destroy(runtime);

    check_server();
}

// Silent Dead Connection test

static void _silent_dead_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)ws;
    volatile int *connected = userdata;
    BB_ASSERT(!BB_FAILED(err));
    *connected = 1;
}

static void websocket_silent_dead_connection_test(void)
{
    printf("\tTesting WebSocket Silent Dead Connection...\n");

    const uint32_t interval_ms = 50;
    const uint32_t max_missed = 2;

    bb_server_set_websocket_heartbeat(server, interval_ms, max_missed);

    volatile int connected = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);
    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);

    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _silent_dead_connect_cb, (void *)&connected);

    while (!connected)
    {
        bb_runtime_tick(runtime);
    }

    /*
     * Simulate a frozen/vanished client: stop ticking its runtime
     * entirely. It never reads the server's pings, never sends a
     * pong, and its socket is never explicitly closed either.
     */

    check_server(); // waits up to 5s for ws_list->head == NULL

    /* Restore default (disabled) so later tests aren't affected. */
    bb_server_set_websocket_heartbeat(server, 0, 0);

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
}

/* ============================================================
 * Raw-socket protocol tests (wire level, bypassing bb_websocket)
 * ============================================================ */

#define RAW_WS_KEY_SAMPLE    "dGhlIHNhbXBsZSBub25jZQ=="
#define RAW_WS_ACCEPT_SAMPLE "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" /* RFC 6455 section 1.3 */

static bb_socket_t raw_ws_connect(int timeout_ms)
{
    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    BB_ASSERT(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);

#ifdef _WIN32
    DWORD tv = (DWORD)timeout_ms;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
    struct timeval tv = {.tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    return fd;
}

/* Reads until the peer closes or the receive timeout fires. */
static size_t raw_ws_read_all(bb_socket_t fd, char *buf, size_t cap)
{
    size_t total = 0;

    while (total + 1 < cap)
    {
        ssize_t n = recv(fd, buf + total, cap - total - 1, 0);
        if (n <= 0)
            break;
        total += (size_t)n;
    }

    buf[total] = '\0';
    return total;
}

static bool raw_ws_contains_ci(const char *haystack, const char *needle)
{
    size_t nlen = strlen(needle);

    for (; *haystack; haystack++)
    {
        if (bb_strncasecmp(haystack, needle, nlen) == 0)
            return true;
    }
    return false;
}

static void raw_ws_send_all(bb_socket_t fd, const void *data, size_t len)
{
    BB_ASSERT(send(fd, data, len, MSG_NOSIGNAL) == (ssize_t)len);
}

/* Reads exactly `len` bytes; false if the peer closed or the timeout fired first. */
static bool raw_ws_recv_exact(bb_socket_t fd, void *buf, size_t len)
{
    size_t got = 0;

    while (got < len)
    {
        ssize_t n = recv(fd, (char *)buf + got, len - got, 0);
        if (n <= 0)
            return false;
        got += (size_t)n;
    }
    return true;
}

/* Sends a handshake for `path` with the RFC sample key and returns the socket
 * after the HTTP response headers have been consumed (so frames follow). */
static bb_socket_t raw_ws_open(const char *path)
{
    bb_socket_t fd = raw_ws_connect(2000);

    char req[512];
    int n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\n"
                     "Host: 127.0.0.1:%d\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "\r\n",
                     path, TEST_PORT);
    raw_ws_send_all(fd, req, (size_t)n);

    /* Byte-wise so nothing after the blank line (i.e. frame data) is swallowed. */
    char resp[1024];
    size_t len = 0;
    while (len < sizeof(resp) - 1)
    {
        BB_ASSERT(raw_ws_recv_exact(fd, resp + len, 1));
        len++;
        if (len >= 4 && memcmp(resp + len - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    resp[len] = '\0';

    BB_ASSERT(strncmp(resp, "HTTP/1.1 101", 12) == 0);
    return fd;
}

/* Builds one masked client frame into `out` (payload <= 65535) and returns its size. */
static size_t raw_ws_build_frame(uint8_t *out, uint8_t first_byte, const void *payload, size_t len)
{
    static const uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
    size_t pos = 0;

    out[pos++] = first_byte;

    if (len < 126)
    {
        out[pos++] = 0x80 | (uint8_t)len;
    }
    else
    {
        out[pos++] = 0x80 | 126;
        out[pos++] = (uint8_t)(len >> 8);
        out[pos++] = (uint8_t)(len & 0xff);
    }

    memcpy(out + pos, mask, 4);
    pos += 4;

    for (size_t i = 0; i < len; i++)
        out[pos + i] = ((const uint8_t *)payload)[i] ^ mask[i & 3];

    return pos + len;
}

/* Reads one server frame (must be unmasked, per RFC 6455 section 5.1). Returns
 * false if the connection closed or timed out before a full frame arrived. */
static bool raw_ws_read_frame(bb_socket_t fd, uint8_t *first_byte, uint8_t *payload, size_t cap, size_t *len_out)
{
    uint8_t hdr[2];
    if (!raw_ws_recv_exact(fd, hdr, 2))
        return false;

    BB_ASSERT((hdr[1] & 0x80) == 0); /* server-to-client frames are never masked */

    size_t len = hdr[1] & 0x7f;
    if (len == 126)
    {
        uint8_t ext[2];
        if (!raw_ws_recv_exact(fd, ext, 2))
            return false;
        len = ((size_t)ext[0] << 8) | ext[1];
    }
    BB_ASSERT(len != 127 && len <= cap); /* tests here never need 64-bit lengths */

    if (!raw_ws_recv_exact(fd, payload, len))
        return false;

    *first_byte = hdr[0];
    *len_out = len;
    return true;
}

/* Expects the next frame to be `first_byte` carrying exactly `expected`. */
static void raw_ws_expect_frame(bb_socket_t fd, uint8_t first_byte, const void *expected, size_t expected_len)
{
    uint8_t buf[1024];
    uint8_t got_first = 0;
    size_t got_len = 0;

    BB_ASSERT(raw_ws_read_frame(fd, &got_first, buf, sizeof(buf), &got_len));
    BB_ASSERT(got_first == first_byte);
    BB_ASSERT(got_len == expected_len);
    BB_ASSERT(expected_len == 0 || memcmp(buf, expected, expected_len) == 0);
}

/* A fresh well-behaved client must still get an echo: proves the server is alive. */
static void raw_ws_assert_echo_works(void)
{
    bb_socket_t fd = raw_ws_open("/echo");

    uint8_t frame[64];
    size_t n = raw_ws_build_frame(frame, 0x81, "still alive", 11);
    raw_ws_send_all(fd, frame, n);
    raw_ws_expect_frame(fd, 0x81, "still alive", 11);

    bb_socket_close(fd);
    check_server();
}

static void websocket_handshake_validation_test(void)
{
    printf("\tTesting WebSocket handshake accept key and invalid upgrade rejection...\n");

    /* ---- valid upgrade: exact RFC 6455 sample accept value ---- */
    {
        bb_socket_t fd = raw_ws_connect(2000);
        const char *req =
            "GET /echo HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";
        raw_ws_send_all(fd, req, strlen(req));

        char resp[1024];
        char *p = resp;
        while ((size_t)(p - resp) < sizeof(resp) - 1)
        {
            BB_ASSERT(raw_ws_recv_exact(fd, p, 1));
            p++;
            if ((p - resp) >= 4 && memcmp(p - 4, "\r\n\r\n", 4) == 0)
                break;
        }
        *p = '\0';

        BB_ASSERT(strncmp(resp, "HTTP/1.1 101", 12) == 0);
        BB_ASSERT(raw_ws_contains_ci(resp, "Upgrade: websocket"));
        BB_ASSERT(raw_ws_contains_ci(resp, "Sec-WebSocket-Accept: " RAW_WS_ACCEPT_SAMPLE));

        bb_socket_close(fd);
        check_server();
    }

    /* ---- invalid upgrades: none may produce a 101 or leave a session behind ---- */
    static const char *const bad_requests[] = {
        /* missing Sec-WebSocket-Key */
        "GET /echo HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n",
        /* unsupported version */
        "GET /echo HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\nSec-WebSocket-Version: 8\r\n\r\n",
        /* missing version */
        "GET /echo HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\n\r\n",
        /* wrong Upgrade token */
        "GET /echo HTTP/1.1\r\nHost: x\r\nUpgrade: h2c\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\nSec-WebSocket-Version: 13\r\n\r\n",
        /* Connection header does not include Upgrade */
        "GET /echo HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: keep-alive\r\n"
        "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\nSec-WebSocket-Version: 13\r\n\r\n",
        /* plain HTTP GET to a websocket route */
        "GET /echo HTTP/1.1\r\nHost: x\r\n\r\n",
        /* valid upgrade headers, but no such route */
        "GET /does-not-exist HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\nSec-WebSocket-Version: 13\r\n\r\n",
    };

    for (size_t i = 0; i < sizeof(bad_requests) / sizeof(bad_requests[0]); i++)
    {
        bb_socket_t fd = raw_ws_connect(500);
        raw_ws_send_all(fd, bad_requests[i], strlen(bad_requests[i]));

        char resp[2048];
        (void)raw_ws_read_all(fd, resp, sizeof(resp));
        bb_socket_close(fd);

        if (strstr(resp, " 101 ") != NULL)
            fprintf(stderr, "bad upgrade request #%zu was accepted:\n%s\n", i, bad_requests[i]);

        BB_ASSERT(strstr(resp, " 101 ") == NULL);
        BB_ASSERT(raw_ws_contains_ci(resp, "Sec-WebSocket-Accept") == false);
    }

    check_server();
    raw_ws_assert_echo_works();
}

static void websocket_control_frames_test(void)
{
    printf("\tTesting WebSocket server answers Ping with Pong and Close with Close...\n");

    bb_socket_t fd = raw_ws_open("/echo");
    uint8_t frame[256];
    size_t n;

    /* Ping payload must be echoed back verbatim in the Pong. */
    n = raw_ws_build_frame(frame, 0x89, "heartbeat", 9);
    raw_ws_send_all(fd, frame, n);
    raw_ws_expect_frame(fd, 0x8A, "heartbeat", 9);

    /* Empty ping, and the 125-byte maximum control payload. */
    n = raw_ws_build_frame(frame, 0x89, "", 0);
    raw_ws_send_all(fd, frame, n);
    raw_ws_expect_frame(fd, 0x8A, "", 0);

    uint8_t max_payload[125];
    for (size_t i = 0; i < sizeof(max_payload); i++)
        max_payload[i] = (uint8_t)('a' + (i % 26));

    n = raw_ws_build_frame(frame, 0x89, max_payload, sizeof(max_payload));
    raw_ws_send_all(fd, frame, n);
    raw_ws_expect_frame(fd, 0x8A, max_payload, sizeof(max_payload));

    /* A ping sandwiched between data frames must not disturb message order. */
    uint8_t burst[256];
    size_t burst_len = 0;
    burst_len += raw_ws_build_frame(burst + burst_len, 0x81, "first", 5);
    burst_len += raw_ws_build_frame(burst + burst_len, 0x89, "mid", 3);
    burst_len += raw_ws_build_frame(burst + burst_len, 0x81, "last", 4);
    raw_ws_send_all(fd, burst, burst_len);

    /* Frames in one batch are processed in order: echo, pong, echo. */
    raw_ws_expect_frame(fd, 0x81, "first", 5);
    raw_ws_expect_frame(fd, 0x8A, "mid", 3);
    raw_ws_expect_frame(fd, 0x81, "last", 4);

    /* Close handshake: server answers with a Close frame, then drops the TCP connection. */
    const uint8_t close_payload[2] = {0x03, 0xE8}; /* 1000 */
    n = raw_ws_build_frame(frame, 0x88, close_payload, sizeof(close_payload));
    raw_ws_send_all(fd, frame, n);

    uint8_t buf[256];
    uint8_t first = 0;
    size_t len = 0;
    BB_ASSERT(raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len));
    BB_ASSERT(first == 0x88);
    BB_ASSERT(len >= 2);
    BB_ASSERT(((buf[0] << 8) | buf[1]) == 1000);

    uint8_t dummy;
    BB_ASSERT(recv(fd, &dummy, 1, 0) <= 0); /* connection closed after the Close frame */

    bb_socket_close(fd);
    check_server();
}

static void websocket_split_and_coalesced_frames_test(void)
{
    printf("\tTesting WebSocket frames split across and coalesced within TCP segments...\n");

    bb_socket_t fd = raw_ws_open("/echo");
    uint8_t frame[1024];
    size_t n;

    /* A 5-byte text frame delivered one byte at a time. */
    n = raw_ws_build_frame(frame, 0x81, "hello", 5);
    for (size_t i = 0; i < n; i++)
    {
        raw_ws_send_all(fd, frame + i, 1);
        bb_usleep(1000);
    }
    raw_ws_expect_frame(fd, 0x81, "hello", 5);

    /* A 300-byte frame (16-bit length) split inside its extended length field
     * (after 3 of the 4 header bytes), then inside the masking key and payload. */
    uint8_t payload[300];
    for (size_t i = 0; i < sizeof(payload); i++)
        payload[i] = (uint8_t)('A' + (i % 26));

    n = raw_ws_build_frame(frame, 0x82, payload, sizeof(payload));
    raw_ws_send_all(fd, frame, 3);
    bb_usleep(20000);
    raw_ws_send_all(fd, frame + 3, 3); /* rest of length + part of the mask key */
    bb_usleep(20000);
    raw_ws_send_all(fd, frame + 6, 100);
    bb_usleep(20000);
    raw_ws_send_all(fd, frame + 106, n - 106);
    raw_ws_expect_frame(fd, 0x82, payload, sizeof(payload));

    /* Three frames in one segment, with the last one cut off mid-payload. */
    uint8_t batch[256];
    size_t batch_len = 0;
    batch_len += raw_ws_build_frame(batch + batch_len, 0x81, "one", 3);
    batch_len += raw_ws_build_frame(batch + batch_len, 0x81, "two", 3);
    size_t third_start = batch_len;
    batch_len += raw_ws_build_frame(batch + batch_len, 0x81, "three", 5);

    raw_ws_send_all(fd, batch, third_start + 8); /* "one", "two", header+mask+1 byte of "three" */
    raw_ws_expect_frame(fd, 0x81, "one", 3);
    raw_ws_expect_frame(fd, 0x81, "two", 3);
    bb_usleep(20000);
    raw_ws_send_all(fd, batch + third_start + 8, batch_len - third_start - 8);
    raw_ws_expect_frame(fd, 0x81, "three", 5);

    bb_socket_close(fd);
    check_server();
}

static void websocket_aborted_and_garbage_input_test(void)
{
    printf("\tTesting WebSocket server survives truncated frames and unknown opcodes...\n");

    /* Peer vanishes mid-frame: header promises 100 bytes, only 10 arrive. */
    for (int i = 0; i < 10; i++)
    {
        bb_socket_t fd = raw_ws_open("/echo");

        uint8_t frame[256];
        uint8_t full[100];
        memset(full, 'x', sizeof(full));
        BB_ASSERT(raw_ws_build_frame(frame, 0x81, full, sizeof(full)) > 16);

        raw_ws_send_all(fd, frame, 6 + 10); /* 2 header + 4 mask + 10 payload bytes */
        bb_usleep(5000);
        bb_socket_close(fd);
    }
    check_server();

    /* Peer vanishes mid-header (a lone first byte) and right after the handshake. */
    {
        bb_socket_t fd = raw_ws_open("/echo");
        const uint8_t lone = 0x81;
        raw_ws_send_all(fd, &lone, 1);
        bb_usleep(5000);
        bb_socket_close(fd);
    }
    {
        bb_socket_t fd = raw_ws_open("/echo");
        bb_socket_close(fd);
    }
    check_server();

    /* Reserved/unknown opcodes must not wedge or crash the session: a valid
     * frame sent afterwards on the same connection is still echoed. */
    {
        bb_socket_t fd = raw_ws_open("/echo");
        uint8_t frame[64];
        size_t n;

        n = raw_ws_build_frame(frame, 0x83, "reserved", 8); /* reserved data opcode 3 */
        raw_ws_send_all(fd, frame, n);
        n = raw_ws_build_frame(frame, 0x8B, "reserved", 8); /* reserved control opcode 11 */
        raw_ws_send_all(fd, frame, n);

        n = raw_ws_build_frame(frame, 0x81, "after", 5);
        raw_ws_send_all(fd, frame, n);

        /* The server may answer or drop the connection; it must never echo the
         * reserved frames back as data. */
        uint8_t buf[64];
        uint8_t first = 0;
        size_t len = 0;
        if (raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len))
        {
            BB_ASSERT((first & 0x0F) != 0x3 && (first & 0x0F) != 0xB);
            BB_ASSERT(!(len == 8 && memcmp(buf, "reserved", 8) == 0));
        }

        bb_socket_close(fd);
        check_server();
    }

    raw_ws_assert_echo_works();
}

/* Sends a masked Pong; used to answer server heartbeat Pings by hand. */
static void raw_ws_send_pong(bb_socket_t fd, const void *payload, size_t len)
{
    uint8_t frame[256];
    size_t n = raw_ws_build_frame(frame, 0x8A, payload, len);
    raw_ws_send_all(fd, frame, n);
}

static void websocket_session_isolation_test(void)
{
    printf("\tTesting WebSocket sessions stay isolated from each other...\n");

    bb_socket_t a = raw_ws_open("/echo");
    bb_socket_t b = raw_ws_open("/echo");
    uint8_t frame[128];
    size_t n;

    /* Interleave traffic: each peer must only ever see its own echoes, in order. */
    for (int i = 0; i < 20; i++)
    {
        char ma[32], mb[32];
        int la = snprintf(ma, sizeof(ma), "A-%d", i);
        int lb = snprintf(mb, sizeof(mb), "B-%d", i);

        n = raw_ws_build_frame(frame, 0x81, ma, (size_t)la);
        raw_ws_send_all(a, frame, n);
        n = raw_ws_build_frame(frame, 0x81, mb, (size_t)lb);
        raw_ws_send_all(b, frame, n);

        raw_ws_expect_frame(b, 0x81, mb, (size_t)lb);
        raw_ws_expect_frame(a, 0x81, ma, (size_t)la);
    }

    /* A leaves with a proper close handshake; B must be unaffected. */
    const uint8_t code[2] = {0x03, 0xE8};
    n = raw_ws_build_frame(frame, 0x88, code, sizeof(code));
    raw_ws_send_all(a, frame, n);

    uint8_t buf[64];
    uint8_t first = 0;
    size_t len = 0;
    BB_ASSERT(raw_ws_read_frame(a, &first, buf, sizeof(buf), &len));
    BB_ASSERT(first == 0x88);
    bb_socket_close(a);

    n = raw_ws_build_frame(frame, 0x81, "B still here", 12);
    raw_ws_send_all(b, frame, n);
    raw_ws_expect_frame(b, 0x81, "B still here", 12);

    /* Then B vanishes abruptly; both sessions must be reaped. */
    bb_socket_close(b);
    check_server();
}

static void websocket_heartbeat_keepalive_test(void)
{
    printf("\tTesting WebSocket heartbeat keeps a responsive client connected...\n");

    const uint32_t interval_ms = 50;
    const uint32_t max_missed = 2;

    bb_server_set_websocket_heartbeat(server, interval_ms, max_missed);

    bb_socket_t fd = raw_ws_open("/echo");

    /* Answer well over max_missed pings. A client that pongs must never be
     * dropped, otherwise healthy-but-idle connections would be killed. */
    for (int i = 0; i < 8; i++)
    {
        uint8_t buf[128];
        uint8_t first = 0;
        size_t len = 0;

        BB_ASSERT(raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len));
        BB_ASSERT(first == 0x89);
        raw_ws_send_pong(fd, buf, len);
    }

    /* Still alive: data frames are echoed (heartbeat Pings may interleave). */
    uint8_t frame[64];
    size_t n = raw_ws_build_frame(frame, 0x81, "alive", 5);
    raw_ws_send_all(fd, frame, n);

    for (;;)
    {
        uint8_t buf[128];
        uint8_t first = 0;
        size_t len = 0;

        BB_ASSERT(raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len));

        if (first == 0x89)
        {
            raw_ws_send_pong(fd, buf, len);
            continue;
        }

        BB_ASSERT(first == 0x81);
        BB_ASSERT(len == 5 && memcmp(buf, "alive", 5) == 0);
        break;
    }

    /* Restore default (disabled) before tearing down. */
    bb_server_set_websocket_heartbeat(server, 0, 0);
    bb_socket_close(fd);
    check_server();
}

static void websocket_close_semantics_test(void)
{
    printf("\tTesting WebSocket Close without a body and data after Close...\n");

    uint8_t frame[128];
    uint8_t buf[64];
    uint8_t first = 0;
    size_t len = 0;
    uint8_t dummy;

    /* A Close frame with no status code is legal (RFC 6455 5.5.1). */
    {
        bb_socket_t fd = raw_ws_open("/echo");
        size_t n = raw_ws_build_frame(frame, 0x88, "", 0);
        raw_ws_send_all(fd, frame, n);

        BB_ASSERT(raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len));
        BB_ASSERT(first == 0x88);
        BB_ASSERT(recv(fd, &dummy, 1, 0) <= 0);

        bb_socket_close(fd);
        check_server();
    }

    /* Frames that follow a Close in the same segment must be ignored: after
     * Close the server may not process or echo any further data. */
    {
        bb_socket_t fd = raw_ws_open("/echo");
        uint8_t seg[128];
        size_t n = 0;
        const uint8_t code[2] = {0x03, 0xE8};

        n += raw_ws_build_frame(seg + n, 0x88, code, sizeof(code));
        n += raw_ws_build_frame(seg + n, 0x81, "late", 4);
        raw_ws_send_all(fd, seg, n);

        BB_ASSERT(raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len));
        BB_ASSERT(first == 0x88);

        /* Nothing else may arrive before the connection is closed. */
        BB_ASSERT(!raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len));

        bb_socket_close(fd);
        check_server();
    }

    raw_ws_assert_echo_works();
}

/* Sends `req` verbatim (optionally one byte at a time) and reads the response
 * headers. Returns true on a 101; the socket is handed back for further use. */
static bool raw_ws_try_handshake(const char *req, bool bytewise, bb_socket_t *fd_out, char *resp, size_t cap)
{
    bb_socket_t fd = raw_ws_connect(1000);
    size_t req_len = strlen(req);

    if (bytewise)
    {
        for (size_t i = 0; i < req_len; i++)
        {
            raw_ws_send_all(fd, req + i, 1);
            bb_usleep(300);
        }
    }
    else
    {
        raw_ws_send_all(fd, req, req_len);
    }

    size_t len = 0;
    while (len < cap - 1)
    {
        if (!raw_ws_recv_exact(fd, resp + len, 1))
            break;
        len++;
        if (len >= 4 && memcmp(resp + len - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    resp[len] = '\0';

    *fd_out = fd;
    return strncmp(resp, "HTTP/1.1 101", 12) == 0;
}

static void websocket_handshake_variants_test(void)
{
    printf("\tTesting WebSocket handshake variants real clients send...\n");

#define WS_REQ_HEAD(path) "GET " path " HTTP/1.1\r\nHost: 127.0.0.1\r\n"
#define WS_REQ_TAIL       "Sec-WebSocket-Key: " RAW_WS_KEY_SAMPLE "\r\nSec-WebSocket-Version: 13\r\n\r\n"

    static const struct
    {
        const char *name;
        const char *req;
        bool bytewise;
    } variants[] = {
        {"handshake delivered one byte at a time",
         WS_REQ_HEAD("/echo") "Upgrade: websocket\r\nConnection: Upgrade\r\n" WS_REQ_TAIL, true},
        {"mixed-case Upgrade value",
         WS_REQ_HEAD("/echo") "Upgrade: WebSocket\r\nConnection: Upgrade\r\n" WS_REQ_TAIL, false},
        {"lower-case Connection value",
         WS_REQ_HEAD("/echo") "Upgrade: websocket\r\nConnection: upgrade\r\n" WS_REQ_TAIL, false},
        {"Connection token list (Firefox style: keep-alive, Upgrade)",
         WS_REQ_HEAD("/echo") "Upgrade: websocket\r\nConnection: keep-alive, Upgrade\r\n" WS_REQ_TAIL, false},
        {"Connection token list with Upgrade first",
         WS_REQ_HEAD("/echo") "Upgrade: websocket\r\nConnection: Upgrade, keep-alive\r\n" WS_REQ_TAIL, false},
        {"browser extras: Origin, subprotocol and extension offers",
         WS_REQ_HEAD("/echo") "Origin: http://example.com\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
         "Sec-WebSocket-Protocol: chat, superchat\r\nSec-WebSocket-Extensions: permessage-deflate\r\n" WS_REQ_TAIL,
         false},
        {"query string on the route",
         WS_REQ_HEAD("/echo?token=abc") "Upgrade: websocket\r\nConnection: Upgrade\r\n" WS_REQ_TAIL, false},
    };

    for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); i++)
    {
        bb_socket_t fd;
        char resp[1024];
        bool upgraded = raw_ws_try_handshake(variants[i].req, variants[i].bytewise, &fd, resp, sizeof(resp));

        if (!upgraded)
            fprintf(stderr, "\t  handshake variant rejected: %s\n", variants[i].name);

        BB_ASSERT(upgraded);
        BB_ASSERT(raw_ws_contains_ci(resp, "Sec-WebSocket-Accept: " RAW_WS_ACCEPT_SAMPLE));

        /* The server implements neither extensions nor subprotocols, so it must
         * not claim one: selecting permessage-deflate here would make every
         * later compressed frame undecodable. */
        BB_ASSERT(!raw_ws_contains_ci(resp, "Sec-WebSocket-Extensions"));
        BB_ASSERT(!raw_ws_contains_ci(resp, "Sec-WebSocket-Protocol"));

        uint8_t frame[64];
        size_t n = raw_ws_build_frame(frame, 0x81, "variant ok", 10);
        raw_ws_send_all(fd, frame, n);
        raw_ws_expect_frame(fd, 0x81, "variant ok", 10);

        bb_socket_close(fd);
    }

#undef WS_REQ_HEAD
#undef WS_REQ_TAIL

    check_server();
}

/* ------------------------------------------------------------------ */
/* Strict RFC 6455 conformance (known gaps: opt in with BB_WS_STRICT_RFC=1) */
/* ------------------------------------------------------------------ */

static int strict_failures = 0;

#define STRICT_CHECK(name, cond)                                         \
    do                                                                   \
    {                                                                    \
        if (!(cond))                                                     \
        {                                                                \
            fprintf(stderr, "\t  STRICT RFC 6455 FAIL: %s\n", (name));   \
            strict_failures++;                                           \
        }                                                                \
    } while (0)

/* After a protocol violation the server must "fail the connection": it may send
 * a Close frame, but nothing else, and it must then drop the TCP connection. */
static bool raw_ws_connection_failed_cleanly(bb_socket_t fd)
{
    for (int i = 0; i < 4; i++)
    {
        uint8_t buf[256];
        uint8_t first = 0;
        size_t len = 0;

        if (!raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len))
            return false; /* closed or timed out */

        if ((first & 0x0F) != 0x8)
            return false; /* echoed data, answered a Ping, etc. */

        uint8_t dummy;
        if (recv(fd, &dummy, 1, 0) <= 0)
            return true;
    }
    return false;
}

static bool raw_ws_violation_rejected(const uint8_t *data, size_t len)
{
    bb_socket_t fd = raw_ws_open("/echo");
    raw_ws_send_all(fd, data, len);
    bool ok = raw_ws_connection_failed_cleanly(fd);
    bb_socket_close(fd);
    return ok;
}

static void websocket_strict_rfc_conformance_test(void)
{
    if (getenv("BB_WS_STRICT_RFC") == NULL)
    {
        printf("\tSkipping strict RFC 6455 conformance tests (set BB_WS_STRICT_RFC=1 to run).\n");
        return;
    }

    printf("\tTesting strict RFC 6455 conformance (fragmentation, masking, UTF-8, control frames)...\n");

    strict_failures = 0;
    uint8_t f[512];
    size_t n;

    /* 5.4: fragmented messages are reassembled into ONE message. */
    {
        bb_socket_t fd = raw_ws_open("/echo");
        n = raw_ws_build_frame(f, 0x01, "hello ", 6);        /* text, FIN=0 */
        n += raw_ws_build_frame(f + n, 0x00, "wor", 3);      /* continuation, FIN=0 */
        n += raw_ws_build_frame(f + n, 0x80, "ld", 2);       /* continuation, FIN=1 */
        raw_ws_send_all(fd, f, n);

        uint8_t buf[256];
        uint8_t first = 0;
        size_t len = 0;
        bool got = raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len);
        STRICT_CHECK("fragmented text message is reassembled and echoed whole",
                     got && first == 0x81 && len == 11 && memcmp(buf, "hello world", 11) == 0);
        bb_socket_close(fd);
    }

    /* 5.4: a control frame may be injected between fragments. */
    {
        bb_socket_t fd = raw_ws_open("/echo");
        n = raw_ws_build_frame(f, 0x01, "ab", 2);
        n += raw_ws_build_frame(f + n, 0x89, "p", 1);
        n += raw_ws_build_frame(f + n, 0x80, "cd", 2);
        raw_ws_send_all(fd, f, n);

        uint8_t buf[256];
        uint8_t first = 0;
        size_t len = 0;
        bool pong = raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len) && first == 0x8A;
        bool text = raw_ws_read_frame(fd, &first, buf, sizeof(buf), &len) &&
                    first == 0x81 && len == 4 && memcmp(buf, "abcd", 4) == 0;
        STRICT_CHECK("Ping between fragments is answered and the message still arrives whole", pong && text);
        bb_socket_close(fd);
    }

    /* 5.1: client frames MUST be masked. */
    {
        const uint8_t unmasked[] = {0x81, 0x03, 'a', 'b', 'c'};
        STRICT_CHECK("unmasked client frame fails the connection",
                     raw_ws_violation_rejected(unmasked, sizeof(unmasked)));
    }

    /* 5.2: RSV bits MUST be 0 without a negotiated extension. */
    {
        n = raw_ws_build_frame(f, 0xC1, "rsv", 3);
        STRICT_CHECK("RSV1 set without an extension fails the connection",
                     raw_ws_violation_rejected(f, n));
    }

    /* 5.2: unknown opcodes fail the connection. */
    {
        n = raw_ws_build_frame(f, 0x83, "x", 1);
        STRICT_CHECK("reserved data opcode fails the connection", raw_ws_violation_rejected(f, n));
        n = raw_ws_build_frame(f, 0x8B, "x", 1);
        STRICT_CHECK("reserved control opcode fails the connection", raw_ws_violation_rejected(f, n));
    }

    /* 5.5: control frames: payload <= 125 bytes and never fragmented. */
    {
        uint8_t big[126];
        memset(big, 'p', sizeof(big));
        n = raw_ws_build_frame(f, 0x89, big, sizeof(big));
        STRICT_CHECK("Ping with a 126-byte payload fails the connection", raw_ws_violation_rejected(f, n));

        n = raw_ws_build_frame(f, 0x09, "x", 1); /* Ping, FIN=0 */
        STRICT_CHECK("fragmented Ping fails the connection", raw_ws_violation_rejected(f, n));
    }

    /* 5.6 / 8.1: text messages MUST be valid UTF-8. */
    {
        n = raw_ws_build_frame(f, 0x81, "\xff\xfe\xfd", 3);
        STRICT_CHECK("invalid UTF-8 in a text message fails the connection", raw_ws_violation_rejected(f, n));
    }

    /* 5.4: a continuation with no message in progress is a protocol error. */
    {
        n = raw_ws_build_frame(f, 0x80, "orphan", 6);
        STRICT_CHECK("orphan continuation frame fails the connection", raw_ws_violation_rejected(f, n));
    }

    /* RFC 9110 5.1: header field names are case-insensitive. */
    {
        bb_socket_t fd;
        char resp[1024];
        const char *lowercase_names =
            "GET /echo HTTP/1.1\r\nhost: 127.0.0.1\r\nupgrade: websocket\r\nconnection: Upgrade\r\n"
            "sec-websocket-key: " RAW_WS_KEY_SAMPLE "\r\nsec-websocket-version: 13\r\n\r\n";

        bool upgraded = raw_ws_try_handshake(lowercase_names, false, &fd, resp, sizeof(resp));
        STRICT_CHECK("handshake with lower-case header names is accepted", upgraded);
        bb_socket_close(fd);
    }

    /* 5.2: a hostile 64-bit length must be rejected without overflowing any
     * size computation (2^64-1 wraps `offset + payload_len`). Runs last: on a
     * vulnerable build this crashes the server process. */
    {
        const uint8_t hostile[] = {0x82, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                   0x01, 0x02, 0x03, 0x04, 'a', 'b'};
        STRICT_CHECK("frame length of 2^64-1 fails the connection", raw_ws_violation_rejected(hostile, sizeof(hostile)));

        const uint8_t huge[] = {0x82, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                0x01, 0x02, 0x03, 0x04, 'a', 'b'};
        STRICT_CHECK("frame length of 2^63-1 fails the connection", raw_ws_violation_rejected(huge, sizeof(huge)));
    }

    check_server();
    raw_ws_assert_echo_works();

    BB_ASSERT(strict_failures == 0);
}

/* ============================================================
 * Main
 * ============================================================ */


// Payload-length encoding boundary tests (RFC 6455 5.2: 7-bit, 16-bit, 64-bit lengths)

static volatile int sized_finished = 0;
static bb_ws_message_type_t sized_type;
static uint8_t *sized_payload = NULL;
static size_t sized_length = 0;

static void _sized_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    BB_ASSERT(!BB_FAILED(err));

    bb_error_t send_err = (sized_type == BB_WS_MESSAGE_TEXT)
        ? bb_websocket_send_text(ws, (const char *)sized_payload)
        : bb_websocket_send_binary(ws, sized_payload, sized_length);

    BB_ASSERT(!BB_FAILED(send_err));
}

static bb_error_t _sized_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == sized_type);
    BB_ASSERT(bb_ws_message_get_length(msg) == sized_length);
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), sized_payload, sized_length) == 0);

    sized_finished = 1;

    return BB_SUCCESS();
}

static void websocket_sized_roundtrip(bb_ws_message_type_t type, size_t length)
{
    printf("\tTesting WebSocket %s round trip of %zu bytes...\n",
           type == BB_WS_MESSAGE_TEXT ? "text" : "binary", length);

    sized_finished = 0;
    sized_type = type;
    sized_length = length;
    sized_payload = malloc(length + 1);
    BB_ASSERT(sized_payload != NULL);

    for (size_t i = 0; i < length; i++)
    {
        /* Text stays printable ASCII (no NUL); binary covers every byte value. */
        sized_payload[i] = (type == BB_WS_MESSAGE_TEXT) ? (uint8_t)('A' + (i % 26)) : (uint8_t)(i * 31 + 7);
    }
    sized_payload[length] = '\0';

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);
    bb_websocket_set_message_callback(client, _sized_message_cb, NULL);
    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _sized_connect_cb, NULL);

    while (!sized_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
    free(sized_payload);
    sized_payload = NULL;
}

// A Pong frame must never crash a session that has no pong callback
// (it used to call through an uninitialised function pointer).

static volatile int pong_test_finished = 0;

static void _pong_connect_cb(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    (void)userdata;

    BB_ASSERT(!BB_FAILED(err));

    /* Unsolicited Pong first, then a text message the server must still echo. */
    BB_ASSERT(!BB_FAILED(bb_websocket_queue_pong(ws, "unsolicited", 11)));
    BB_ASSERT(!BB_FAILED(bb_websocket_send_text(ws, "after pong")));
}

static bb_error_t _pong_message_cb(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;

    BB_ASSERT(bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT);
    BB_ASSERT(bb_ws_message_get_length(msg) == strlen("after pong"));
    BB_ASSERT(memcmp(bb_ws_message_get_data(msg), "after pong", strlen("after pong")) == 0);

    pong_test_finished = 1;

    return BB_SUCCESS();
}

static void websocket_unsolicited_pong_test(void)
{
    printf("\tTesting WebSocket unsolicited Pong does not crash the server...\n");

    pong_test_finished = 0;

    bb_runtime_t *runtime = bb_runtime_create();
    bb_runtime_set_running(runtime);

    bb_websocket_t *client = bb_websocket_create_on_runtime(runtime);
    bb_websocket_set_message_callback(client, _pong_message_cb, NULL);
    bb_websocket_connect(client, "ws://127.0.0.1:8081/echo", _pong_connect_cb, NULL);

    while (!pong_test_finished)
    {
        bb_runtime_tick(runtime);
    }

    bb_websocket_destroy(client);
    bb_runtime_destroy(runtime);
}

static void websocket_length_boundary_test(void)
{
    /* 125: 7-bit, 126: first 16-bit, 65535: last 16-bit, 65536: first 64-bit. */
    static const size_t sizes[] = { 125, 126, 65535, 65536, 70000, 300000 };

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        websocket_sized_roundtrip(BB_WS_MESSAGE_TEXT, sizes[i]);
        websocket_sized_roundtrip(BB_WS_MESSAGE_BINARY, sizes[i]);
    }
}

int main(void)
{
    pthread_t thread;

    BB_ASSERT(pthread_create(&thread, NULL, server_thread, NULL) == 0);

    while (!server)
    {
        bb_usleep(10000);
    }

    printf("Testing websocket client/server integration:\n");

    websocket_echo_test();
    websocket_multi_message_test();
    websocket_large_message_test();
    websocket_binary_message_test();
    websocket_large_binary_test();
    websocket_length_boundary_test();
    websocket_unsolicited_pong_test();
    websocket_sequential_connections_test();
    websocket_multiple_clients_test();
    websocket_many_messages_test();
    websocket_ping_pong_test();
    websocket_close_test();
    websocket_dead_connection_test();
    websocket_multiple_dead_connections_test();
    websocket_silent_dead_connection_test();
    websocket_handshake_validation_test();
    websocket_control_frames_test();
    websocket_split_and_coalesced_frames_test();
    websocket_aborted_and_garbage_input_test();
    websocket_session_isolation_test();
    websocket_heartbeat_keepalive_test();
    websocket_close_semantics_test();
    websocket_handshake_variants_test();
    websocket_strict_rfc_conformance_test();

    bb_runtime_stop(server_runtime);
    printf("All websocket integration tests passed.\n");

    pthread_join(thread, NULL);
    return 0;
}
