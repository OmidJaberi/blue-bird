#ifndef BB_CONNECTION_ASYNC_TASKS_H
#define BB_CONNECTION_ASYNC_TASKS_H

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/error/error.h"
#include "connection.h"

//Disconnect
typedef void (*bb_async_close_fn)(void *userdata);

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

    bool disconnected;

    bb_task_t *write_task;
    int write_watch_events;    // which BB_EVENT_* the write task is currently registered for
    bool write_rewatching;     // true while cancelling+recreating write_task for a direction flip

    bb_async_callback_t write_success;
    bb_async_callback_t write_failure;

    void *write_userdata;


    bb_task_t *read_task;
    int read_watch_events;     // which BB_EVENT_* the read task is currently registered for

    bb_read_step_fn read_step;
    bb_read_error_fn read_error;

    void *read_userdata;

    bb_async_close_fn disconnect;
    void *disconnect_userdata;

} bb_async_connection_t;

bb_async_connection_t *bb_async_connection_create(bb_runtime_t *runtime);
void bb_async_connection_destroy(bb_async_connection_t *async_conn);

void bb_async_connection_set_disconnect_callback(bb_async_connection_t *async_conn, bb_async_close_fn callback, void *userdata);

bb_async_connection_t *bb_async_connection_serve(bb_runtime_t *runtime, int port);
bb_async_connection_t *bb_async_connection_accept(bb_runtime_t *runtime, bb_socket_t server_fd);

/* Same as bb_async_connection_accept(), but immediately upgrades the
 * accepted connection to server-side TLS (see bb_connection_upgrade_to_tls()).
 * The connection starts life in BB_CONNECTION_HANDSHAKE; the read task
 * created via bb_async_connection_create_read_task() drives the
 * handshake transparently before any HTTP/WebSocket bytes are seen. */
bb_async_connection_t *bb_async_connection_accept_tls(bb_runtime_t *runtime, bb_socket_t server_fd, bb_tls_context_t *tls_ctx);

bb_async_connection_t *bb_async_connection_connect(bb_runtime_t *runtime, const char *host, const char *port_str);
void bb_async_connection_close(bb_async_connection_t *async_conn);

bb_error_t bb_async_connection_create_write_task(bb_async_connection_t *async_conn, bb_async_callback_t success, bb_async_callback_t failure, void *userdata);
bb_error_t bb_async_connection_create_read_task(bb_async_connection_t *async_conn, bb_read_step_fn read_step, bb_read_error_fn read_error, void *userdata);

void bb_async_connection_pause_read(bb_async_connection_t *async_conn);

#endif // BB_CONNECTION_ASYNC_TASKS_H
