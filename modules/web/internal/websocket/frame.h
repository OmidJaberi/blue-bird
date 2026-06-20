#ifndef BB_INTERNAL_WS_FRAME_H
#define BB_INTERNAL_WS_FRAME_H

#include <stdint.h>

#include "blue-bird/error/error.h"

typedef struct {
    uint8_t fin;
    uint8_t opcode;

    uint8_t masked;

    uint64_t payload_length;

    uint8_t masking_key[4];

    char *payload;
} bb_ws_frame_t;

void bb_ws_frame_destroy(bb_ws_frame_t *frame);

#endif
