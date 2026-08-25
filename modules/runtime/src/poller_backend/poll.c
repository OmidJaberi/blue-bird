#include <stdlib.h>

#include "poller_backend.h"

#if defined(_WIN32)
typedef WSAPOLLFD _bb_pollfd_t;
#define _bb_poll_fn(fds, nfds, timeout) WSAPoll((fds), (nfds), (timeout))
#else
#include <poll.h>
#include <errno.h>
typedef struct pollfd _bb_pollfd_t;
#define _bb_poll_fn(fds, nfds, timeout) poll((fds), (nfds), (timeout))
#endif

/*
 * Neither poll() nor WSAPoll() has kernel-side persistent registration
 * the way epoll/kqueue do -- there's nothing to create, and register/
 * unregister have no OS-level work to do at all. bb_poller_wait() just
 * rebuilds an ephemeral pollfd array from the common fds[] table (owned
 * entirely by poller.c) on every call.
 */

int _bb_poller_backend_create(bb_poller_t *poller)
{
    (void)poller;
    return 0;
}

void _bb_poller_backend_destroy(bb_poller_t *poller)
{
    (void)poller;
}

int _bb_poller_backend_register(bb_poller_t *poller, bb_socket_t fd, int events)
{
    (void)poller;
    (void)fd;
    (void)events;
    return 0;
}

int _bb_poller_backend_unregister(bb_poller_t *poller, bb_socket_t fd, int events)
{
    (void)poller;
    (void)fd;
    (void)events;
    return 0;
}

int _bb_poller_backend_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
    if (poller->count == 0)
    {
        return 0;
    }

    _bb_pollfd_t *pfds = malloc((size_t)poller->count * sizeof(*pfds));

    if (!pfds)
    {
        return -1;
    }

    for (int i = 0; i < poller->count; i++)
    {
        pfds[i].fd = poller->fds[i].fd;
        pfds[i].events = 0;
        pfds[i].revents = 0;

        if (poller->fds[i].events & BB_EVENT_READ)  pfds[i].events |= POLLIN;
        if (poller->fds[i].events & BB_EVENT_WRITE) pfds[i].events |= POLLOUT;
    }

    int n;
#if defined(_WIN32)
    n = _bb_poll_fn(pfds, (ULONG)poller->count, timeout_ms);
#else
    do
    {
        n = _bb_poll_fn(pfds, (nfds_t)poller->count, timeout_ms);
    } while (n < 0 && errno == EINTR);
#endif

    if (n <= 0)
    {
        free(pfds);
        return n;
    }

    int event_count = 0;

    for (int i = 0; i < poller->count && event_count < max_events; i++)
    {
        int triggered = 0;

        if (pfds[i].revents & POLLIN)  triggered |= BB_EVENT_READ;
        if (pfds[i].revents & POLLOUT) triggered |= BB_EVENT_WRITE;

        if (pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            triggered |= poller->fds[i].events;
        }

        if (triggered)
        {
            events[event_count].fd = poller->fds[i].fd;
            events[event_count].events = triggered;
            event_count++;
        }
    }

    free(pfds);

    return event_count;
}
