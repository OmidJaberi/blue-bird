#ifndef BB_INTERNAL_WEBSOCKET_H
#define BB_INTERNAL_WEBSOCKET_H

#include "connection.h"
#include "websocket/frame.h"
#include "blue-bird/error/error.h"

typedef enum {
    BB_WEBSOCKET_SERVER,
    BB_WEBSOCKET_CLIENT
} bb_websocket_mode_t;

typedef struct bb_websocket {
    bb_connection_t *connection;
    bb_websocket_mode_t mode;
} bb_websocket_t;

// Lifecycle
bb_websocket_t *bb_websocket_create(bb_connection_t *connection, bb_websocket_mode_t mode);
void bb_websocket_destroy(bb_websocket_t *ws);

// Frame operations
bb_error_t bb_websocket_read_frame(bb_websocket_t *ws, bb_ws_frame_t *frame);

bb_error_t bb_websocket_queue_frame(bb_websocket_t *ws, const bb_ws_frame_t *frame);

void bb_ws_frame_destroy(bb_ws_frame_t *frame);

// Convenience helpers
bb_error_t bb_websocket_send_text(bb_websocket_t *ws, const char *text);

bb_error_t bb_websocket_send_binary(bb_websocket_t *ws, const void *data, size_t length);

bb_error_t bb_websocket_send_ping(bb_websocket_t *ws);

bb_error_t bb_websocket_send_pong(bb_websocket_t *ws);

bb_error_t bb_websocket_send_close(bb_websocket_t *ws);

#endif
