#ifndef BB_INTERNAL_WS_FRAME_H
#define BB_INTERNAL_WS_FRAME_H

#include <stdint.h>

#include "blue-bird/error/error.h"
#include "blue-bird/web/websocket/message.h"

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

bb_error_t bb_ws_frame_to_message(const bb_ws_frame_t *frame, bb_ws_message_t *message);
void bb_ws_frame_destroy(bb_ws_frame_t *frame);

#endif
