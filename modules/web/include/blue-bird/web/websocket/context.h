#ifndef BB_WS_CONTEXT_H
#define BB_WS_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

#include "blue-bird/error/error.h"

typedef struct bb_ws_context bb_ws_context_t;

// Sends a text message.
bb_error_t bb_ws_send_text(bb_ws_context_t *ctx, const char *text);

// Sends a binary message.
bb_error_t bb_ws_send_binary(bb_ws_context_t *ctx, const void *data, size_t length);

// Closes the websocket connection.
bb_error_t bb_ws_close(bb_ws_context_t *ctx);

// Gets user-defined context data.
void *bb_ws_userdata(bb_ws_context_t *ctx);

// Sets user-defined context data.
void bb_ws_set_userdata(bb_ws_context_t *ctx, void *userdata);


#ifdef __cplusplus
}
#endif

#endif
