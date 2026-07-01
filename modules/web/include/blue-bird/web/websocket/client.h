#ifndef BB_WS_CLIENT_H
#define BB_WS_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/websocket/websocket.h"

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/error/error.h"
#include "message.h"

typedef struct bb_ws_client bb_ws_client_t;

typedef void (*bb_ws_connect_cb)(bb_ws_client_t *client, bb_error_t err, void *userdata);

bb_ws_client_t *bb_ws_client_create(void);

bb_ws_client_t *bb_ws_client_create_on_runtime(bb_runtime_t *runtime);

void bb_ws_client_destroy(bb_ws_client_t *client);

void bb_ws_client_close(bb_ws_client_t *client);

void bb_ws_client_connect_async(bb_ws_client_t *client, const char *url, bb_ws_connect_cb callback, void *userdata);

bb_error_t bb_ws_client_send_text(bb_ws_client_t *client, const char *text);

bb_error_t bb_ws_client_send_binary(bb_ws_client_t *client, const void *data, size_t length);

void bb_ws_client_set_message_callback(bb_ws_client_t *client, bb_ws_handler_cb callback, void *userdata);


#ifdef __cplusplus
}
#endif

#endif
