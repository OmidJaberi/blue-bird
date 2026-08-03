#ifndef BB_WS_MESSAGE_H
#define BB_WS_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

typedef enum {
    BB_WS_MESSAGE_TEXT,
    BB_WS_MESSAGE_BINARY,
    BB_WS_MESSAGE_CLOSE,
    BB_WS_MESSAGE_PING,
    BB_WS_MESSAGE_PONG,
} bb_ws_message_type_t;

typedef struct bb_ws_message {
    bb_ws_message_type_t type;

    const void *data;
    size_t length;
} bb_ws_message_t;


#ifdef __cplusplus
}
#endif

#endif
