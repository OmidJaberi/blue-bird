#ifndef BB_ASYNC_CONNECTION_H
#define BB_ASYNC_CONNECTION_H

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"
#include "blue-bird/web/client.h"
#include "websocket/session.h"
#include "connection.h"

typedef struct {
    bb_server_t *server;
    bb_connection_t *connection;
    bb_runtime_t *runtime;
    bb_ws_session_t *ws_session;
} bb_server_task_data_t;

typedef struct {
    bb_client_t *client;
    bb_client_callback_t callback;
    void *userdata;
} _bb_client_task_data_t;

void bb_accept_task(bb_task_t *task, void *userdata);
void bb_client_create_write_task(_bb_client_task_data_t *client_data);

#endif // BB_ASYNC_CONNECTION_H
