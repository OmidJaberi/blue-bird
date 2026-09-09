#ifndef BB_TRANSPORT_H
#define BB_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif


#include <blue-bird/utils/platform.h>
#include "blue-bird/web/error.h"
#include "blue-bird/web/tls.h"

#include <stddef.h>

/*
 * bb_transport_t is what sits underneath bb_connection_t: it turns
 * "read/write bytes on this fd" into either plain recv()/send() or
 * OpenSSL's SSL_read()/SSL_write(), while presenting the exact same
 * non-blocking, event-driven contract to the caller either way.
 *
 * HTTP/WebSocket code (and even bb_connection_t's callers) never see
 * OpenSSL types or error codes -- everything OpenSSL-specific is
 * translated into this status enum by the TLS transport implementation.
 */
typedef enum {
    BB_TRANSPORT_OK,
    BB_TRANSPORT_WANT_READ,
    BB_TRANSPORT_WANT_WRITE,
    BB_TRANSPORT_CLOSED,
    BB_TRANSPORT_ERROR
} bb_transport_status_t;

typedef struct bb_transport bb_transport_t;

typedef struct {
    /* Optional. NULL means "nothing to negotiate, always succeeds". */
    bb_transport_status_t (*handshake)(bb_transport_t *transport);

    bb_transport_status_t (*read)(bb_transport_t *transport, void *buffer, size_t capacity, size_t *bytes_read);
    bb_transport_status_t (*write)(bb_transport_t *transport, const void *buffer, size_t length, size_t *bytes_written);

    /* Optional. NULL means "nothing to do, always succeeds". Called
     * best-effort and non-blocking; callers must not loop on it. */
    bb_transport_status_t (*shutdown)(bb_transport_t *transport);

    /* Mandatory. Must free `transport` itself. Must never close `fd` --
     * bb_connection_t owns the socket lifetime independently of which
     * transport happens to be layered over it. */
    void (*destroy)(bb_transport_t *transport);
} bb_transport_ops_t;

struct bb_transport {
    const bb_transport_ops_t *ops;
    bb_socket_t fd;
    void *impl;
};

/* Dispatch helpers -- bb_connection_t (and tests) should only ever go
 * through these, never touch `ops` directly. */
bb_transport_status_t bb_transport_handshake(bb_transport_t *transport);
bb_transport_status_t bb_transport_read(bb_transport_t *transport, void *buffer, size_t capacity, size_t *bytes_read);
bb_transport_status_t bb_transport_write(bb_transport_t *transport, const void *buffer, size_t length, size_t *bytes_written);
bb_transport_status_t bb_transport_shutdown(bb_transport_t *transport);
void bb_transport_destroy(bb_transport_t *transport);

/* Plain TCP transport. Always available; preserves pre-TLS behavior
 * exactly (same recv()/send() semantics as before the transport layer
 * existed). */
bb_transport_t *bb_transport_create_tcp(bb_socket_t fd);

/*
 * TLS/TCP transport.
 *
 * Real implementation lives in transport/tls.c and is only compiled in
 * when BB_WITH_TLS is enabled at build time; otherwise transport/tls_stub.c
 * provides the same symbols and simply fails with BB_ERR_TLS_UNSUPPORTED.
 * This keeps the rest of the web module (server.c, connection.c,
 * async_connection.c) free of #ifdef BB_WITH_TLS -- the API surface is
 * identical either way, only its capability differs.
 */
typedef struct bb_tls_context bb_tls_context_t;

/* Loads and validates the certificate/key, builds an SSL_CTX with
 * secure defaults, and fails loudly (never at first-connection time)
 * if anything is wrong. */
bb_tls_context_t *bb_tls_context_create_server(const bb_tls_config_t *config, bb_error_t *out_err);
void bb_tls_context_destroy(bb_tls_context_t *ctx);

/* One TLS/TCP transport per accepted connection, sharing the server's
 * SSL_CTX. Begins in the "server, handshake not started" state; the
 * first bb_transport_handshake() call drives SSL_accept(). */
bb_transport_t *bb_transport_create_tls_server(bb_socket_t fd, bb_tls_context_t *ctx);


#ifdef __cplusplus
}
#endif

#endif //BB_TRANSPORT_H
