#ifndef BB_WEBSOCKET_H
#define BB_WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stddef.h>

#include "connection.h"
#include "blue-bird/error/error.h"

typedef enum {
    BB_WS_CONTINUATION = 0x0,
    BB_WS_TEXT         = 0x1,
    BB_WS_BINARY       = 0x2,
    BB_WS_CLOSE        = 0x8,
    BB_WS_PING         = 0x9,
    BB_WS_PONG         = 0xA
} bb_ws_opcode_t;

typedef struct {
    uint8_t fin;
    uint8_t opcode;

    uint8_t masked;

    uint64_t payload_length;

    uint8_t masking_key[4];

    char *payload;
} bb_ws_frame_t;

typedef struct bb_websocket {
    bb_connection_t *connection;
} bb_websocket_t;


// Lifecycle
bb_websocket_t *bb_websocket_create(bb_connection_t *connection);
void bb_websocket_destroy(bb_websocket_t *ws);

// Frame operations
bb_error_t bb_websocket_read_frame(bb_websocket_t *ws, bb_ws_frame_t *frame);

bb_error_t bb_websocket_write_frame(bb_websocket_t *ws, const bb_ws_frame_t *frame);

void bb_ws_frame_destroy(bb_ws_frame_t *frame);

// Convenience helpers
bb_error_t bb_websocket_send_text(bb_websocket_t *ws, const char *text);

bb_error_t bb_websocket_send_binary(bb_websocket_t *ws, const void *data, size_t length);

bb_error_t bb_websocket_send_ping(bb_websocket_t *ws);

bb_error_t bb_websocket_send_pong(bb_websocket_t *ws);

bb_error_t bb_websocket_send_close(bb_websocket_t *ws);


#ifdef __cplusplus
}
#endif

#endif
