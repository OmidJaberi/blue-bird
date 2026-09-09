#include "transport/transport.h"

#include <stdlib.h>
#include <limits.h>

/* --------------------------------------------------------------------- */
/* Dispatch                                                               */
/* --------------------------------------------------------------------- */

bb_transport_status_t bb_transport_handshake(bb_transport_t *transport)
{
    if (!transport || !transport->ops)
        return BB_TRANSPORT_ERROR;

    if (!transport->ops->handshake)
        return BB_TRANSPORT_OK;

    return transport->ops->handshake(transport);
}

bb_transport_status_t bb_transport_read(bb_transport_t *transport, void *buffer, size_t capacity, size_t *bytes_read)
{
    if (!transport || !transport->ops || !transport->ops->read)
        return BB_TRANSPORT_ERROR;

    return transport->ops->read(transport, buffer, capacity, bytes_read);
}

bb_transport_status_t bb_transport_write(bb_transport_t *transport, const void *buffer, size_t length, size_t *bytes_written)
{
    if (!transport || !transport->ops || !transport->ops->write)
        return BB_TRANSPORT_ERROR;

    return transport->ops->write(transport, buffer, length, bytes_written);
}

bb_transport_status_t bb_transport_shutdown(bb_transport_t *transport)
{
    if (!transport || !transport->ops)
        return BB_TRANSPORT_OK;

    if (!transport->ops->shutdown)
        return BB_TRANSPORT_OK;

    return transport->ops->shutdown(transport);
}

void bb_transport_destroy(bb_transport_t *transport)
{
    if (!transport)
        return;

    if (transport->ops && transport->ops->destroy)
    {
        transport->ops->destroy(transport);
        return;
    }

    free(transport);
}

/* --------------------------------------------------------------------- */
/* Plain TCP transport                                                    */
/* --------------------------------------------------------------------- */

static bb_transport_status_t _tcp_read(bb_transport_t *transport, void *buffer, size_t capacity, size_t *bytes_read)
{
    /* recv()/send() take a signed int length on Windows (size_t
     * elsewhere); clamp rather than let the implicit narrowing warn or,
     * in principle, wrap. */
    int want = (capacity > INT_MAX) ? INT_MAX : (int)capacity;

    ssize_t n = recv(transport->fd, buffer, want, 0);

    if (n > 0)
    {
        *bytes_read = (size_t)n;
        return BB_TRANSPORT_OK;
    }

    /* Peer performed an orderly shutdown (EOF). */
    if (n == 0)
        return BB_TRANSPORT_CLOSED;

    if (bb_socket_would_block())
        return BB_TRANSPORT_WANT_READ;

    if (bb_socket_connection_closed())
        return BB_TRANSPORT_CLOSED;

    return BB_TRANSPORT_ERROR;
}

static bb_transport_status_t _tcp_write(bb_transport_t *transport, const void *buffer, size_t length, size_t *bytes_written)
{
    int want = (length > INT_MAX) ? INT_MAX : (int)length;

    ssize_t n = send(transport->fd, buffer, want, MSG_NOSIGNAL);

    if (n > 0)
    {
        *bytes_written = (size_t)n;
        return BB_TRANSPORT_OK;
    }

    if (bb_socket_would_block())
        return BB_TRANSPORT_WANT_WRITE;

    if (bb_socket_connection_closed())
        return BB_TRANSPORT_CLOSED;

    return BB_TRANSPORT_ERROR;
}

static void _tcp_destroy(bb_transport_t *transport)
{
    /* Never closes transport->fd -- bb_connection_t owns the socket. */
    free(transport);
}

static const bb_transport_ops_t _bb_tcp_transport_ops = {
    .handshake = NULL,
    .read = _tcp_read,
    .write = _tcp_write,
    .shutdown = NULL,
    .destroy = _tcp_destroy,
};

bb_transport_t *bb_transport_create_tcp(bb_socket_t fd)
{
    bb_transport_t *transport = calloc(1, sizeof(*transport));
    if (!transport)
        return NULL;

    transport->ops = &_bb_tcp_transport_ops;
    transport->fd = fd;
    transport->impl = NULL;

    return transport;
}
