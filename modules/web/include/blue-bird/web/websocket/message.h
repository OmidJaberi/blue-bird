#ifndef BB_WS_MESSAGE_H
#define BB_WS_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

typedef enum {
    BB_WS_MESSAGE_TEXT,
    BB_WS_MESSAGE_BINARY
} bb_ws_message_type_t;

typedef struct {
    bb_ws_message_type_t type;

    const void *data;
    size_t length;
} bb_ws_message_t;

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


#ifdef __cplusplus
}
#endif

#endif
