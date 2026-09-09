/*
 * OpenSSL-backed implementation of bb_tls_context_t / the TLS transport.
 *
 * This is the *only* file in Blue-Bird that is allowed to touch OpenSSL
 * APIs directly. Everything else (bb_connection_t, bb_async_connection_t,
 * server.c, the HTTP parser, WebSocket code...) only ever sees
 * bb_transport_t and bb_transport_status_t.
 */

#include "transport/transport.h"

#include "blue-bird/log/log.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct bb_tls_context {
    SSL_CTX *ssl_ctx;
};

typedef struct {
    SSL *ssl;
} _bb_tls_transport_impl_t;

/* --------------------------------------------------------------------- */
/* Library init / error plumbing                                         */
/* --------------------------------------------------------------------- */

static void _bb_tls_lib_init(void)
{
    static bool initialized = false;

    if (initialized)
        return;

    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
    initialized = true;
}

/* Drains and logs the OpenSSL error queue; never sent to remote peers,
 * this is purely for local diagnostics via Blue-Bird's own logger. */
static bb_error_t _bb_tls_error(bb_error_code_t code, const char *context_msg)
{
    unsigned long e = ERR_get_error();

    if (e != 0)
    {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        BB_LOG_ERROR("%s: %s\n", context_msg, buf);

        /* Drain any remaining queued errors so they don't leak into the
         * next unrelated operation's diagnostics. */
        while (ERR_get_error() != 0) { }
    }
    else
    {
        BB_LOG_ERROR("%s\n", context_msg);
    }

    return BB_ERROR(code, context_msg);
}

/* --------------------------------------------------------------------- */
/* Context (SSL_CTX) creation                                            */
/* --------------------------------------------------------------------- */

bb_tls_context_t *bb_tls_context_create_server(const bb_tls_config_t *config, bb_error_t *out_err)
{
    if (out_err)
        *out_err = BB_SUCCESS();

    if (!config || !config->certificate_file || !config->private_key_file)
    {
        if (out_err)
            *out_err = BB_ERROR(BB_ERR_TLS_CONFIG, "TLS certificate and private key files are required.");
        return NULL;
    }

    _bb_tls_lib_init();

    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx)
    {
        if (out_err)
            *out_err = _bb_tls_error(BB_ERR_TLS_CONFIG, "Failed to create TLS context.");
        return NULL;
    }

    /* Secure defaults: no SSLv3/TLS1.0/1.1, no compression (CRIME),
     * tolerate partial/moving write buffers so retried SSL_write() calls
     * after WANT_WRITE don't require pointer-identical buffers. */
    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, config->certificate_file) != 1)
    {
        if (out_err)
            *out_err = _bb_tls_error(BB_ERR_TLS_CONFIG, "Failed to load TLS certificate.");
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, config->private_key_file, SSL_FILETYPE_PEM) != 1)
    {
        if (out_err)
            *out_err = _bb_tls_error(BB_ERR_TLS_CONFIG, "Failed to load TLS private key.");
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    if (SSL_CTX_check_private_key(ssl_ctx) != 1)
    {
        if (out_err)
            *out_err = _bb_tls_error(BB_ERR_TLS_CONFIG, "TLS certificate and private key do not match.");
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    bb_tls_context_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
    {
        if (out_err)
            *out_err = BB_ERROR(BB_ERR_ALLOC, "Failed to allocate TLS context.");
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    ctx->ssl_ctx = ssl_ctx;
    return ctx;
}

void bb_tls_context_destroy(bb_tls_context_t *ctx)
{
    if (!ctx)
        return;

    SSL_CTX_free(ctx->ssl_ctx);
    free(ctx);
}

/* --------------------------------------------------------------------- */
/* Transport (per-connection SSL object)                                 */
/* --------------------------------------------------------------------- */

static bb_transport_status_t _bb_tls_map_ssl_error(SSL *ssl, int ret)
{
    int err = SSL_get_error(ssl, ret);

    switch (err)
    {
        case SSL_ERROR_WANT_READ:
            return BB_TRANSPORT_WANT_READ;

        case SSL_ERROR_WANT_WRITE:
            return BB_TRANSPORT_WANT_WRITE;

        case SSL_ERROR_ZERO_RETURN:
            /* Peer sent close_notify: orderly TLS-level shutdown. */
            return BB_TRANSPORT_CLOSED;

        case SSL_ERROR_SYSCALL:
            /* ret == 0 with no queued OpenSSL error means the underlying
             * socket hit EOF/was reset before a close_notify arrived. */
            if (ERR_peek_error() == 0)
            {
                BB_LOG_ERROR("TLS connection closed abruptly (no close_notify).\n");
                return BB_TRANSPORT_CLOSED;
            }
            _bb_tls_error(BB_ERR_IO, "TLS I/O syscall error");
            return BB_TRANSPORT_ERROR;

        case SSL_ERROR_SSL:
            _bb_tls_error(BB_ERR_TLS_HANDSHAKE, "TLS protocol error");
            return BB_TRANSPORT_ERROR;

        default:
            BB_LOG_ERROR("Unhandled TLS error condition (%d).\n", err);
            return BB_TRANSPORT_ERROR;
    }
}

static bb_transport_status_t _bb_tls_handshake(bb_transport_t *transport)
{
    _bb_tls_transport_impl_t *impl = transport->impl;

    int ret = SSL_do_handshake(impl->ssl);
    if (ret == 1)
        return BB_TRANSPORT_OK;

    return _bb_tls_map_ssl_error(impl->ssl, ret);
}

static bb_transport_status_t _bb_tls_read(bb_transport_t *transport, void *buffer, size_t capacity, size_t *bytes_read)
{
    _bb_tls_transport_impl_t *impl = transport->impl;

    /* SSL_read takes a signed int length; clamp rather than truncate
     * unpredictably on very large capacities. */
    int want = (capacity > INT_MAX) ? INT_MAX : (int)capacity;

    int ret = SSL_read(impl->ssl, buffer, want);
    if (ret > 0)
    {
        *bytes_read = (size_t)ret;
        return BB_TRANSPORT_OK;
    }

    return _bb_tls_map_ssl_error(impl->ssl, ret);
}

static bb_transport_status_t _bb_tls_write(bb_transport_t *transport, const void *buffer, size_t length, size_t *bytes_written)
{
    _bb_tls_transport_impl_t *impl = transport->impl;

    int want = (length > INT_MAX) ? INT_MAX : (int)length;

    int ret = SSL_write(impl->ssl, buffer, want);
    if (ret > 0)
    {
        *bytes_written = (size_t)ret;
        return BB_TRANSPORT_OK;
    }

    return _bb_tls_map_ssl_error(impl->ssl, ret);
}

static bb_transport_status_t _bb_tls_shutdown(bb_transport_t *transport)
{
    _bb_tls_transport_impl_t *impl = transport->impl;

    int ret = SSL_shutdown(impl->ssl);
    if (ret == 1)
        return BB_TRANSPORT_OK;

    if (ret == 0)
    {
        /* Our close_notify was sent; the peer's hasn't arrived yet. This
         * is a normal half of a bidirectional shutdown, not an error --
         * the caller treats this as "best effort, don't loop on it". */
        return BB_TRANSPORT_WANT_READ;
    }

    return _bb_tls_map_ssl_error(impl->ssl, ret);
}

static void _bb_tls_destroy(bb_transport_t *transport)
{
    if (!transport)
        return;

    _bb_tls_transport_impl_t *impl = transport->impl;
    if (impl)
    {
        /* SSL_free() does not close the fd: SSL_set_fd() wraps it in a
         * BIO_NOCLOSE socket BIO, so ownership stays with bb_connection_t. */
        if (impl->ssl)
            SSL_free(impl->ssl);
        free(impl);
    }

    free(transport);
}

static const bb_transport_ops_t _bb_tls_transport_ops = {
    .handshake = _bb_tls_handshake,
    .read = _bb_tls_read,
    .write = _bb_tls_write,
    .shutdown = _bb_tls_shutdown,
    .destroy = _bb_tls_destroy,
};

bb_transport_t *bb_transport_create_tls_server(bb_socket_t fd, bb_tls_context_t *ctx)
{
    if (!ctx)
        return NULL;

    _bb_tls_transport_impl_t *impl = calloc(1, sizeof(*impl));
    if (!impl)
        return NULL;

    impl->ssl = SSL_new(ctx->ssl_ctx);
    if (!impl->ssl)
    {
        _bb_tls_error(BB_ERR_TLS_HANDSHAKE, "Failed to create TLS session.");
        free(impl);
        return NULL;
    }

    if (SSL_set_fd(impl->ssl, (int)fd) != 1)
    {
        _bb_tls_error(BB_ERR_TLS_HANDSHAKE, "Failed to bind TLS session to socket.");
        SSL_free(impl->ssl);
        free(impl);
        return NULL;
    }

    SSL_set_accept_state(impl->ssl);

    bb_transport_t *transport = calloc(1, sizeof(*transport));
    if (!transport)
    {
        SSL_free(impl->ssl);
        free(impl);
        return NULL;
    }

    transport->ops = &_bb_tls_transport_ops;
    transport->fd = fd;
    transport->impl = impl;

    return transport;
}
