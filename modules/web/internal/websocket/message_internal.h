#ifndef BB_INTERNAL_WEBSOCKET_MESSAGE_H
#define BB_INTERNAL_WEBSOCKET_MESSAGE_H


#include "blue-bird/web/websocket/message.h"

/*
 * Initializes a message structure.
 */
void bb_ws_message_init(bb_ws_message_t *message, bb_ws_message_type_t type, const void *data, size_t length);

/*
 * Clears a message structure.
 *
 * Currently a no-op because the message does not
 * own its payload, but exists for API consistency
 * and future extensibility.
 */
void bb_ws_message_destroy(bb_ws_message_t *message);


#endif
