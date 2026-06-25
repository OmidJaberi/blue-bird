#include "blue-bird/web/websocket/client.h"

#include "connection.h"
#include "http/parser.h"
#include "websocket/websocket_internal.h"

#include "blue-bird/runtime/event.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct bb_ws_client {
    bb_runtime_t *runtime;

    bb_connection_t *connection;
    bb_websocket_t *websocket;

    bb_ws_message_cb message_cb;
    void *message_userdata;
};

bb_ws_client_t *bb_ws_client_create_on_runtime(bb_runtime_t *runtime)
{
    if (!runtime)
    {
        return NULL;
    }

    bb_ws_client_t *client = calloc(1, sizeof(*client));

    if (!client)
    {
        return NULL;
    }

    client->runtime = runtime;

    return client;
}

bb_ws_client_t *bb_ws_client_create(void)
{
    return bb_ws_client_create_on_runtime(bb_runtime_default());
}

void bb_ws_client_close(bb_ws_client_t *client)
{
    if (!client)
    {
        return;
    }

    if (client->websocket)
    {
        bb_websocket_destroy(client->websocket);
        client->websocket = NULL;
    }

    if (client->connection)
    {
        bb_connection_destroy(client->connection);
        client->connection = NULL;
    }
}

void bb_ws_client_destroy(bb_ws_client_t *client)
{
    if (!client)
    {
        return;
    }

    bb_ws_client_close(client);

    free(client);
}
