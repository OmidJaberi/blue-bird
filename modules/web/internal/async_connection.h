#ifndef BB_ASYNC_CONNECTION_H
#define BB_ASYNC_CONNECTION_H

#include "blue-bird/web/server.h"
#include "blue-bird/web/client.h"

bb_error_t bb_server_create_accept_task(bb_server_t *server);
bb_error_t bb_client_create_write_task(bb_client_t *client, bb_client_callback_t callback, void *userdata);

#endif // BB_ASYNC_CONNECTION_H
