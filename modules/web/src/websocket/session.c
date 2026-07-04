#include "websocket/session.h"

#include <stdlib.h>

bb_ws_session_t *bb_ws_session_create(bb_connection_t *connection, bb_ws_handler_cb handler)
{
    bb_ws_session_t *session = calloc(1, sizeof(*session));

    if (!session)
    {
        return NULL;
    }

    session->connection = connection;

    session->websocket = bb_websocket_create(connection, BB_WEBSOCKET_SERVER);

    if (!session->websocket)
    {
        free(session);
        return NULL;
    }

    session->handler = handler;

    return session;
}

void bb_ws_session_destroy(bb_ws_session_t *session)
{
    if (!session)
    {
        return;
    }

    bb_websocket_destroy(session->websocket);

    free(session);
}
