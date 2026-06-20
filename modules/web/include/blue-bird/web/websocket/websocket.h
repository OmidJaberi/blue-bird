#ifndef BB_WEBSOCKET_H
#define BB_WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "blue-bird/web/http/handler.h"
#include "blue-bird/error/error.h"

#include "connection.h"
#include "message.h"
#include "context.h"

typedef bb_error_t (*bb_ws_handler_cb)(bb_ws_context_t *ctx, const bb_ws_message_t *message);

bool bb_websocket_is_upgrade_request(bb_request_t *req);

char *bb_websocket_accept_key(const char *client_key);

bb_error_t bb_websocket_accept(bb_request_t *req, bb_response_t *res);


#ifdef __cplusplus
}
#endif

#endif
