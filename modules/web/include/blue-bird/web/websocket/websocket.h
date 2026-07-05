#ifndef BB_WEBSOCKET_H
#define BB_WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/error/error.h"
#include "blue-bird/runtime/runtime.h"

#include "message.h"

typedef struct bb_websocket bb_websocket_t;

typedef bb_error_t (*bb_ws_handler_cb)(bb_websocket_t *ws, const bb_ws_message_t *message);
typedef void (*bb_ws_connect_cb)(bb_websocket_t *ws, bb_error_t err, void *userdata);

// Lifecycle
bb_websocket_t *bb_websocket_create_on_runtime(bb_runtime_t *runtime);

static inline bb_websocket_t *bb_websocket_create()
{
    return bb_websocket_create_on_runtime(bb_runtime_default());
}

void bb_websocket_destroy(bb_websocket_t *ws);

// Connection
void bb_websocket_connect(bb_websocket_t *ws, const char *url, bb_ws_connect_cb connect_callback, void *userdata);

void bb_websocket_set_message_callback(bb_websocket_t *ws, bb_ws_handler_cb callback, void *userdata);


bb_error_t bb_websocket_send_text(bb_websocket_t *ws, const char *text);

bb_error_t bb_websocket_send_binary(bb_websocket_t *ws, const void *data, size_t length);

bb_error_t bb_websocket_send_ping(bb_websocket_t *ws);

bb_error_t bb_websocket_send_pong(bb_websocket_t *ws);

bb_error_t bb_websocket_send_close(bb_websocket_t *ws);


#ifdef __cplusplus
}
#endif

#endif
