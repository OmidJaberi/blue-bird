#ifndef BB_INTERNAL_WS_CONTEXT_H
#define BB_INTERNAL_WS_CONTEXT_H

#include "blue-bird/web/websocket/context.h"
#include "websocket_internal.h"

struct bb_ws_context {
    bb_websocket_t *websocket;
};

#endif
