#ifndef BB_ASYNC_CONNECTION_H
#define BB_ASYNC_CONNECTION_H

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/server.h"
#include "connection.h"

typedef struct {
    bb_server_t *server;
    bb_runtime_t *runtime;
    bb_connection_t *connection;
} _bb_accept_task_data_t;

void bb_accept_task(bb_task_t *task, void *userdata);

#endif // BB_ASYNC_CONNECTION_H
