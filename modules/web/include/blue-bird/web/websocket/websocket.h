#ifndef BB_WEBSOCKET_H
#define BB_WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/error/error.h"

#include "message.h"

typedef struct bb_websocket bb_websocket_t;

typedef bb_error_t (*bb_ws_handler_cb)(bb_websocket_t *ws, const bb_ws_message_t *message);

bb_error_t bb_websocket_send_text(bb_websocket_t *ws, const char *text);

bb_error_t bb_websocket_send_binary(bb_websocket_t *ws, const void *data, size_t length);

bb_error_t bb_websocket_send_ping(bb_websocket_t *ws);

bb_error_t bb_websocket_send_pong(bb_websocket_t *ws);

bb_error_t bb_websocket_send_close(bb_websocket_t *ws);


#ifdef __cplusplus
}
#endif

#endif
