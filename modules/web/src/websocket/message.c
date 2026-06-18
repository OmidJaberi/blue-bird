#include "blue-bird/web/websocket/message.h"

void bb_ws_message_init(bb_ws_message_t *message, bb_ws_message_type_t type, const void *data, size_t length)
{
    if (!message)
    {
        return;
    }

    message->type = type;
    message->data = data;
    message->length = length;
}

void bb_ws_message_destroy(bb_ws_message_t *message)
{
    (void)message;
}
