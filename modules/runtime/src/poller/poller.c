#include <stdlib.h>

#include "poller_internal.h"

/* ======================================================================= */
/* Shared lookup helper                                                    */
/* ======================================================================= */

int _bb_find_fd(_bb_poll_fd_t *fds, int count, bb_socket_t fd)
{
    for (int i = 0; i < count; i++)
    {
        if (fds[i].fd == fd)
        {
            return i;
        }
    }

    return -1;
}

/* ======================================================================= */
/* Public API -- all common semantics/bookkeeping live here. Every backend */
/* only performs the actual OS-level operation and reports success/       */
/* failure; it never touches poller->fds[]/poller->count directly.        */
/* ======================================================================= */

bb_poller_t *bb_poller_create(void)
{
    bb_poller_t *poller = calloc(1, sizeof(bb_poller_t));

    if (!poller)
    {
        return NULL;
    }

    if (_bb_poller_backend_create(poller) != 0)
    {
        free(poller);
        return NULL;
    }

    return poller;
}

void bb_poller_destroy(bb_poller_t *poller)
{
    if (!poller)
    {
        return;
    }

    _bb_poller_backend_destroy(poller);
    free(poller);
}

int bb_poller_register(bb_poller_t *poller, bb_socket_t fd, int events)
{
    if (!poller)
    {
        return -1;
    }

    if (!bb_poller_fd_supported(fd))
    {
        return -1;
    }

    int idx = _bb_find_fd(poller->fds, poller->count, fd);

    if (idx < 0 && poller->count >= BB_POLLER_MAX_FDS)
    {
        return -1;
    }

    if (_bb_poller_backend_register(poller, fd, events) != 0)
    {
        return -1;
    }

    if (idx >= 0)
    {
        poller->fds[idx].events |= events;
    }
    else
    {
        poller->fds[poller->count].fd = fd;
        poller->fds[poller->count].events = events;
        poller->count++;
    }

    return 0;
}

int bb_poller_unregister(bb_poller_t *poller, bb_socket_t fd, int events)
{
    if (!poller)
    {
        return -1;
    }

    int idx = _bb_find_fd(poller->fds, poller->count, fd);

    if (idx < 0)
    {
        return -1;
    }

    /* Best-effort at the OS level (matches historical behavior of never
     * checking the removal syscall's return value); our own bookkeeping
     * below is always kept consistent regardless. */
    _bb_poller_backend_unregister(poller, fd, events);

    poller->fds[idx].events &= ~events;

    if (poller->fds[idx].events == 0)
    {
        poller->fds[idx] = poller->fds[poller->count - 1];
        poller->count--;
    }

    return 0;
}

int bb_poller_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
    if (!poller || !events)
    {
        return -1;
    }

    return _bb_poller_backend_wait(poller, events, max_events, timeout_ms);
}
