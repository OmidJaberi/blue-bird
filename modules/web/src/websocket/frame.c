#include "websocket/frame.h"

#include <stdlib.h>
#include <string.h>

bb_error_t bb_ws_frame_to_message(const bb_ws_frame_t *frame, bb_ws_message_t *message)
{
    if (!frame || !message)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }
    switch (frame->opcode)
    {
        case BB_WS_TEXT:
            message->type = BB_WS_MESSAGE_TEXT;
            break;

        case BB_WS_BINARY:
            message->type = BB_WS_MESSAGE_BINARY;
            break;

        default:
            return BB_ERROR(BB_ERR_INTERNAL, "Unsupported opcode");
    }
    message->data = frame->payload;
    message->length = frame->payload_length;
    return BB_SUCCESS();
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
