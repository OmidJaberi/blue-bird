#ifndef BB_CONNECTION_H
#define BB_CONNECTION_H

#ifdef __cplusplus
extern "C" {
#endif


#include <blue-bird/utils/platform.h>
#include "blue-bird/web/error.h"
#include "transport/transport.h"

#include <stddef.h>
#include <stdbool.h>

/*
 * Hard upper bound on how large a single connection's read buffer is
 * ever allowed to grow. Without this, a client that trickles in bytes
 * (or declares a huge Content-Length) forever without ever completing
 * a request forces bb_connection_read() to keep doubling its buffer
 * indefinitely, giving a single connection an unbounded claim on the
 * heap -- a trivial denial-of-service vector when many such connections
 * are opened at once. Once a connection's buffer would need to grow
 * past this cap, bb_connection_read() rejects the payload with
 * BB_ERR_PAYLOAD_TOO_LARGE instead of allocating further.
 */
#define BB_CONNECTION_MAX_BUFFER_SIZE (4 * 1024 * 1024) /* 4 MiB */

typedef enum {
    BB_CONNECTION_HANDSHAKE, // TLS handshake in progress; no HTTP/WS parsing yet
    BB_CONNECTION_READING,
    BB_CONNECTION_WRITING,
    BB_CONNECTION_CLOSED
} bb_connection_state_t;

/*
 * Which readiness direction bb_connection_read()/bb_connection_write()
 * are actually waiting on right now. Normally reads wait on READ and
 * writes wait on WRITE, but TLS can need either direction for either
 * operation (handshake, renegotiation, or a partial SSL_write that
 * needs a socket read to make progress). BB_CONN_IO_DEFAULT means
 * "the obvious direction for this operation"; the async connection
 * layer flips the poller's watched event only when it sees NEED_READ
 * on a write, or NEED_WRITE on a read.
 */
typedef enum {
    BB_CONN_IO_DEFAULT,
    BB_CONN_IO_NEED_READ,
    BB_CONN_IO_NEED_WRITE
} bb_conn_io_hint_t;

typedef struct write_buffer {
    char *write_buffer;
    size_t write_length;
    size_t write_offset;
    struct write_buffer *next;
} write_buffer_t;

typedef struct bb_connection {
    bb_socket_t fd;

    bb_connection_state_t state;

    // Read buffer
    char *buffer;
    size_t buffer_length;
    size_t buffer_capacity;

    // Write buffer
    write_buffer_t *write_data;
    bool write_pending;

    // Transport (plain TCP or TLS/TCP) that bb_connection_read/write
    // actually go through. Always non-NULL after a successful create.
    bb_transport_t *transport;
    bb_conn_io_hint_t read_io_hint;
    bb_conn_io_hint_t write_io_hint;

    void *userdata;
} bb_connection_t;

bb_connection_t *bb_connection_create(bb_socket_t client_fd);
bb_connection_t *bb_connection_create_non_blocking(bb_socket_t fd);
void bb_connection_destroy(bb_connection_t *connection);

int bb_connection_buffer_add(bb_connection_t *connection, char *buffer, size_t length);

bb_connection_t *bb_connection_serve(int port);
bb_connection_t *bb_connection_accept(bb_socket_t server_fd);
bb_connection_t *bb_connection_connect(const char *host, const char *port_str);
bb_connection_t *bb_connection_connect_nonblocking(const char *host, const char *port_str);
bb_error_t bb_connection_read(bb_connection_t *connection);
bb_error_t bb_connection_write(bb_connection_t *connection);

/*
 * Replaces the connection's transport with a server-side TLS/TCP
 * transport bound to the same fd, and puts the connection into
 * BB_CONNECTION_HANDSHAKE. Nothing above bb_connection_t (HTTP parser,
 * router, WebSocket code) is aware this happened -- bb_connection_read()
 * simply returns no data until the handshake completes.
 *
 * Returns 0 on success, -1 on failure (connection is left unchanged).
 */
int bb_connection_upgrade_to_tls(bb_connection_t *connection, bb_tls_context_t *tls_ctx);


#ifdef __cplusplus
}
#endif

#endif
