#ifndef BB_WEBSOCKET_H
#define BB_WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/error/error.h"

#include "message.h"
#include "context.h"

typedef bb_error_t (*bb_ws_handler_cb)(bb_ws_context_t *ctx, const bb_ws_message_t *message);


#ifdef __cplusplus
}
#endif

#endif
