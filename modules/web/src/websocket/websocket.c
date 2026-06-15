#include "websocket.h"

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

bb_websocket_t *bb_websocket_create(bb_connection_t *connection)
{
    if (!connection)
    {
        return NULL;
    }

    bb_websocket_t *ws = malloc(sizeof(*ws));

    if (!ws)
    {
        return NULL;
    }

    ws->connection = connection;

    return ws;
}

void bb_websocket_destroy(bb_websocket_t *ws)
{
    free(ws);
}

void bb_ws_frame_destroy(bb_ws_frame_t *frame)
{
    if (!frame)
    {
        return;
    }

    free(frame->payload);

    memset(frame, 0, sizeof(*frame));
}