#include "websocket/frame.h"
#include "websocket/message_internal.h"

#include <stdlib.h>
#include <string.h>

bb_ws_message_t *bb_ws_frame_to_message(const bb_ws_frame_t *frame)
{
    if (!frame)
    {
        return NULL;
    }
    bb_ws_message_type_t msg_type;
    switch (frame->opcode)
    {
        case BB_WS_TEXT:
            msg_type = BB_WS_MESSAGE_TEXT;
            break;

        case BB_WS_BINARY:
            msg_type = BB_WS_MESSAGE_BINARY;
            break;

        case BB_WS_CLOSE:
            msg_type = BB_WS_MESSAGE_CLOSE;
            break;

        case BB_WS_PING:
            msg_type = BB_WS_MESSAGE_PING;
            break;

        case BB_WS_PONG:
            msg_type = BB_WS_MESSAGE_PONG;
            break;

        default: // BB_WS_CONTINUATION ...
            return NULL;
    }
    return bb_ws_message_create(msg_type, frame->payload, frame->payload_length);
}

void bb_ws_frame_destroy(bb_ws_frame_t *frame)
{
    if (!frame)
    {
        return;
    }

    bb_ws_frame_t *next = frame->next;
    if (next)
    {
        bb_ws_frame_destroy(next);
        free(next);
    }

    free(frame->payload);

    memset(frame, 0, sizeof(*frame));
}
