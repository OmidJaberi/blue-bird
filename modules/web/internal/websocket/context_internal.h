#ifndef BB_INTERNAL_WS_CONTEXT_H
#define BB_INTERNAL_WS_CONTEXT_H

#include "blue-bird/web/websocket/context.h"
#include "websocket_internal.h"

struct bb_ws_context {
    bb_websocket_t *websocket;
};

bb_ws_context_t *bb_ws_context_create(bb_websocket_t *websocket);
void bb_ws_context_destroy(bb_ws_context_t *ctx);

#endif
