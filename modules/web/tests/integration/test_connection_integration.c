#include "connection/connection.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define TEST_PORT 18080

static volatile int finished = 0;
static bb_connection_t *server_listener = NULL;

/* ============================================================
 * Server
 * ============================================================ */

static void *server_thread(void *arg)
{
    (void)arg;

    server_listener = bb_connection_serve(TEST_PORT);
    assert(server_listener != NULL);

    while (!finished)
    {
        usleep(1000);
    }

    bb_connection_destroy(server_listener);

    return NULL;
}

/* ============================================================
 * Tests
 * ============================================================ */

static void connection_test(void)
{
    printf("\tTesting connection establishment...\n");

    bb_connection_t *client = bb_connection_connect_nonblocking("127.0.0.1", "18080");

    assert(client != NULL);

    bb_connection_t *server = NULL;

    while (!server)
    {
        server = bb_connection_accept(server_listener->fd);
        usleep(1000);
    }

    assert(server != NULL);

    bb_connection_destroy(client);
    bb_connection_destroy(server);
}

static void client_to_server_test(void)
{
    printf("\tTesting client -> server transfer...\n");

    bb_connection_t *client = bb_connection_connect_nonblocking("127.0.0.1", "18080");

    assert(client);

    bb_connection_t *server = NULL;

    while (!server)
    {
        server = bb_connection_accept(server_listener->fd);
        usleep(1000);
    }

    char *msg = malloc(6);
    memcpy(msg, "hello", 6);

    assert(bb_connection_buffer_add(client, msg, 6) == 0);
    assert(bb_connection_write(client) == 1);

    while (server->buffer_length < 6)
    {
        bb_connection_read(server);
        usleep(1000);
    }

    assert(server->buffer_length == 6);
    assert(memcmp(server->buffer, "hello", 6) == 0);

    bb_connection_destroy(client);
    bb_connection_destroy(server);
}

static void server_to_client_test(void)
{
    printf("\tTesting server -> client transfer...\n");

    bb_connection_t *client = bb_connection_connect_nonblocking("127.0.0.1", "18080");

    assert(client);

    bb_connection_t *server = NULL;

    while (!server)
    {
        server = bb_connection_accept(server_listener->fd);
        usleep(1000);
    }

    char *msg = malloc(6);
    memcpy(msg, "world", 6);

    assert(bb_connection_buffer_add(server, msg, 6) == 0);
    assert(bb_connection_write(server) == 1);

    while (client->buffer_length < 6)
    {
        bb_connection_read(client);
        usleep(1000);
    }

    assert(client->buffer_length == 6);
    assert(memcmp(client->buffer, "world", 6) == 0);

    bb_connection_destroy(client);
    bb_connection_destroy(server);
}

#define LARGE_SIZE 10000

static void large_transfer_test(void)
{
    printf("\tTesting large transfer...\n");

    bb_connection_t *client = bb_connection_connect_nonblocking("127.0.0.1", "18080");

    assert(client);

    bb_connection_t *server = NULL;

    while (!server)
    {
        server = bb_connection_accept(server_listener->fd);
        usleep(1000);
    }

    char *buffer = malloc(LARGE_SIZE);

    memset(buffer, 'A', LARGE_SIZE);

    assert(bb_connection_buffer_add(client, buffer, LARGE_SIZE) == 0);
    assert(bb_connection_write(client) == 1);

    while (server->buffer_length < LARGE_SIZE)
    {
        bb_connection_read(server);
        usleep(1000);
    }

    assert(server->buffer_length == LARGE_SIZE);

    for (int i = 0; i < LARGE_SIZE; i++)
    {
        assert(server->buffer[i] == 'A');
    }

    assert(server->buffer_capacity >= LARGE_SIZE);

    bb_connection_destroy(client);
    bb_connection_destroy(server);
}

int main(void)
{
    pthread_t thread;

    assert(pthread_create(&thread, NULL, server_thread, NULL) == 0);

    while (!server_listener)
    {
        usleep(1000);
    }

    printf("Running connection integration tests...\n");

    connection_test();
    client_to_server_test();
    server_to_client_test();
    large_transfer_test();

    finished = 1;

    pthread_join(thread, NULL);

    printf("All connection integration tests passed.\n");

    return 0;
}
