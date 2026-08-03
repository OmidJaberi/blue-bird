#ifndef BB_INTERNAL_WEBSOCKET_MESSAGE_H
#define BB_INTERNAL_WEBSOCKET_MESSAGE_H


#include "blue-bird/web/websocket/message.h"

/*
 * Creates a message structure.
 */
bb_ws_message_t *bb_ws_message_create(bb_ws_message_type_t type, const void *data, size_t length);

/*
 * Clears and destroys a message structure.
 */
void bb_ws_message_destroy(bb_ws_message_t *message);


#endif
