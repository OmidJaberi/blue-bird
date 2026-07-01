#ifndef BB_ASYNC_CONNECTION_H
#define BB_ASYNC_CONNECTION_H

#include "blue-bird/web/server.h"
#include "blue-bird/web/client.h"
#include "blue-bird/web/websocket/websocket.h"

//Write
typedef void (*bb_async_callback_t)(bb_task_t *, void *);

//Read
typedef enum {
    BB_READ_MORE,
    BB_READ_DONE,
    BB_READ_ERROR,
} bb_read_result_t;

typedef struct {
    bb_read_result_t result;
    bb_error_t err;
} bb_read_status_t;

typedef bb_read_status_t (*bb_read_step_fn)(void *userdata);
typedef void (*bb_read_error_fn)(bb_error_t err, void *userdata);

bb_error_t bb_server_create_accept_task(bb_server_t *server);
bb_error_t bb_connection_task_create_write(bb_runtime_t *runtime, bb_connection_t *conn, bb_async_callback_t success, bb_async_callback_t failure, void *userdata);
bb_error_t bb_connection_task_create_read(bb_runtime_t *runtime, bb_connection_t *connection, bb_read_step_fn read_step, bb_read_error_fn read_error, void *userdata);
bb_error_t bb_connection_task_create_ws_read(bb_runtime_t *runtime, bb_connection_t *connection, bb_ws_handler_cb handler);

#endif // BB_ASYNC_CONNECTION_H
