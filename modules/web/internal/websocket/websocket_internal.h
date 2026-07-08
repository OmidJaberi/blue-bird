#ifndef BB_INTERNAL_WEBSOCKET_H
#define BB_INTERNAL_WEBSOCKET_H

#include "blue-bird/web/websocket/websocket.h"
#include "websocket/frame.h"
#include "blue-bird/web/http/handler.h"
#include "connection/async_tasks.h"
#include "blue-bird/error/error.h"
#include "blue-bird/runtime/runtime.h"

#include <stdbool.h>

typedef enum {
    BB_WEBSOCKET_SERVER,
    BB_WEBSOCKET_CLIENT
} bb_websocket_mode_t;

typedef struct bb_websocket {
    bb_runtime_t *runtime;
    bb_async_connection_t *async_conn;
    bb_websocket_mode_t mode;
    bb_ws_handler_cb handler; // message_cb
    void *message_userdata;
    bb_ws_pong_cb pong_cb;
    void *pong_userdata;
} bb_websocket_t;

// Accept
char *bb_websocket_accept_key(const char *client_key);
bb_websocket_t *bb_websocket_accept(bb_async_connection_t *async_conn, bb_request_t *req, bb_response_t *res, bb_ws_handler_cb handler);

// Lifecycle
bb_websocket_t *bb_websocket_create_with_type(bb_async_connection_t *async_conn, bb_websocket_mode_t mode);

// Frame operations
bb_error_t bb_websocket_read_frames(bb_websocket_t *ws, bb_ws_frame_t *frame);

bb_error_t bb_websocket_queue_frame(bb_websocket_t *ws, const bb_ws_frame_t *frame);

void bb_ws_frame_destroy(bb_ws_frame_t *frame);

// Convenience helpers
bb_error_t bb_websocket_queue_text(bb_websocket_t *ws, const char *text);

bb_error_t bb_websocket_queue_binary(bb_websocket_t *ws, const void *data, size_t length);

bb_error_t bb_websocket_queue_ping(bb_websocket_t *ws, const void *payload, size_t length);

bb_error_t bb_websocket_queue_pong(bb_websocket_t *ws, const void *payload, size_t length);

bb_error_t bb_websocket_queue_close(bb_websocket_t *ws, uint16_t code, const char *reason);

// Async Task
bb_error_t bb_websocket_create_read_task(bb_websocket_t *ws);


#endif
