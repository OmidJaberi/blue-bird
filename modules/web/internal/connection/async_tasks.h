#ifndef BB_CONNECTION_ASYNC_TASKS_H
#define BB_CONNECTION_ASYNC_TASKS_H

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/error/error.h"
#include "connection.h"

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

typedef struct bb_async_connection {
    bb_runtime_t *runtime;
    bb_connection_t *connection;


    bb_task_t *write_task;

    bb_async_callback_t write_success;
    bb_async_callback_t write_failure;

    void *write_userdata;


    bb_task_t *read_task;

    bb_read_step_fn read_step;
    bb_read_error_fn read_error;

    void *read_userdata;
} bb_async_connection_t;

bb_async_connection_t *bb_async_connection_create(bb_runtime_t *runtime);
void bb_async_connection_destroy(bb_async_connection_t *async_conn);

bb_async_connection_t *bb_async_connection_serve(bb_runtime_t *runtime, int port);
bb_async_connection_t *bb_async_connection_accept(bb_runtime_t *runtime, int server_fd);
void bb_async_connection_close(bb_async_connection_t *async_conn);

bb_error_t bb_async_connection_create_write_task(bb_async_connection_t *async_conn, bb_async_callback_t success, bb_async_callback_t failure, void *userdata);
bb_error_t bb_async_connection_create_read_task(bb_async_connection_t *async_conn, bb_read_step_fn read_step, bb_read_error_fn read_error, void *userdata);

// OLD API
bb_error_t bb_connection_task_create_write(bb_runtime_t *runtime, bb_connection_t *conn, bb_async_callback_t success, bb_async_callback_t failure, void *userdata);
bb_error_t bb_connection_task_create_read(bb_runtime_t *runtime, bb_connection_t *connection, bb_read_step_fn read_step, bb_read_error_fn read_error, void *userdata);

#endif // BB_CONNECTION_ASYNC_TASKS_H
