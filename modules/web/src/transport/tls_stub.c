/*
 * Stand-in for tls.c when Blue-Bird is built with -DBB_WITH_TLS=OFF
 * (the default). Keeps bb_tls_context_create_server()/
 * bb_transport_create_tls_server() linkable so the rest of the web
 * module -- and application code calling bb_server_create_tls*() --
 * never needs `#ifdef BB_WITH_TLS`. TLS configuration simply fails
 * with a clear, actionable error instead of silently downgrading to
 * plaintext.
 */

#include "transport/transport.h"

struct bb_tls_context {
    int _unused;
};

bb_tls_context_t *bb_tls_context_create_server(const bb_tls_config_t *config, bb_error_t *out_err)
{
    (void) config;

    if (out_err)
    {
        *out_err = BB_ERROR(
            BB_ERR_TLS_UNSUPPORTED,
            "Blue-Bird was built without TLS support. Rebuild with -DBB_WITH_TLS=ON (requires OpenSSL).");
    }

    return NULL;
}

void bb_tls_context_destroy(bb_tls_context_t *ctx)
{
    (void) ctx;
}

bb_transport_t *bb_transport_create_tls_server(bb_socket_t fd, bb_tls_context_t *ctx)
{
    (void) fd;
    (void) ctx;

    /* Unreachable in practice: bb_tls_context_create_server() above
     * always fails first, so no caller should ever have a non-NULL
     * bb_tls_context_t to pass in here. */
    return NULL;
}
