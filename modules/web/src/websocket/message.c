#include "websocket/message_internal.h"

#include <stdlib.h>
#include <string.h>

bb_ws_message_t *bb_ws_message_create(bb_ws_message_type_t type, const void *data, size_t length)
{
    bb_ws_message_t *message = calloc(1, sizeof(*message));
    if (!message)
    {
        return NULL;
    }
    message->data = NULL;
    message->type = type;
    message->length = length;

    if (type == BB_WS_MESSAGE_TEXT)
    {
        message->data = strdup(data);
    }
    else if (length > 0)
    {
        message->data = malloc(length);
        if (!message->data)
        {
            free(message);
            return NULL;
        }

        memcpy(message->data, data, length);
    }
    return message;
}

void bb_ws_message_destroy(bb_ws_message_t *message)
{
    if (!message)
    {
        return;
    }
    if (message->data)
    {
        free(message->data);
    }
    free(message);
}

bb_ws_message_type_t bb_ws_message_get_type(const bb_ws_message_t *message)
{
    return message->type;
}

void *bb_ws_message_get_data(const bb_ws_message_t *message)
{
    return message->data;
}

size_t bb_ws_message_get_length(const bb_ws_message_t *message)
{
    return message->length;
}
