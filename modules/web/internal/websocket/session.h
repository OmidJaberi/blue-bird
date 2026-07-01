#ifndef BB_INTERNAL_WS_SESSION_H
#define BB_INTERNAL_WS_SESSION_H

#include "blue-bird/web/websocket/websocket.h"
#include "context_internal.h"

#include "connection/connection.h"
#include "websocket_internal.h"

typedef struct bb_ws_session {
    bb_connection_t *connection;
    bb_websocket_t *websocket;
    bb_ws_context_t context;
    bb_ws_handler_cb handler;
} bb_ws_session_t;

bb_ws_session_t *bb_ws_session_create(bb_connection_t *connection, bb_ws_handler_cb handler);

void bb_ws_session_destroy(bb_ws_session_t *session);

#endif
