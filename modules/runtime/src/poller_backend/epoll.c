#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "poller_backend.h"

static uint32_t _to_epoll_events(int events)
{
    uint32_t e = 0;

    if (events & BB_EVENT_READ)  e |= EPOLLIN;
    if (events & BB_EVENT_WRITE) e |= EPOLLOUT;

    return e;
}

static int _from_epoll_events(uint32_t events, int interest)
{
    int triggered = 0;

    if (events & EPOLLIN)  triggered |= BB_EVENT_READ;
    if (events & EPOLLOUT) triggered |= BB_EVENT_WRITE;

    /* EPOLLERR/EPOLLHUP are always reported regardless of the interest
     * mask; surface them on whichever side(s) the caller actually asked
     * about, so a hangup/error is never silently dropped. */
    if (events & (EPOLLERR | EPOLLHUP))
    {
        triggered |= interest;
    }

    return triggered & interest;
}

int _bb_poller_backend_create(bb_poller_t *poller)
{
    poller->epfd = epoll_create1(0);

    return poller->epfd >= 0 ? 0 : -1;
}

void _bb_poller_backend_destroy(bb_poller_t *poller)
{
    close(poller->epfd);
}

int _bb_poller_backend_register(bb_poller_t *poller, bb_socket_t fd, int events)
{
    int idx = _bb_find_fd(poller->fds, poller->count, fd);
    int existing = idx >= 0 ? poller->fds[idx].events : 0;
    int merged = existing | events;

    struct epoll_event ev = {0};
    ev.events = _to_epoll_events(merged);
    ev.data.fd = fd;

    int op = idx >= 0 ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    return epoll_ctl(poller->epfd, op, fd, &ev) == 0 ? 0 : -1;
}

int _bb_poller_backend_unregister(bb_poller_t *poller, bb_socket_t fd, int events)
{
    int idx = _bb_find_fd(poller->fds, poller->count, fd);

    if (idx < 0)
    {
        return -1;
    }

    int remaining = poller->fds[idx].events & ~events;

    struct epoll_event ev = {0};

    if (remaining == 0)
    {
        epoll_ctl(poller->epfd, EPOLL_CTL_DEL, fd, &ev);
    }
    else
    {
        ev.events = _to_epoll_events(remaining);
        ev.data.fd = fd;
        epoll_ctl(poller->epfd, EPOLL_CTL_MOD, fd, &ev);
    }

    return 0;
}

int _bb_poller_backend_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
    struct epoll_event *epevents = malloc((size_t)max_events * sizeof(*epevents));

    if (!epevents)
    {
        return -1;
    }

    int n;
    do
    {
        n = epoll_wait(poller->epfd, epevents, max_events, timeout_ms);
    } while (n < 0 && errno == EINTR);

    if (n <= 0)
    {
        free(epevents);
        return n;
    }

    int event_count = 0;

    for (int i = 0; i < n; i++)
    {
        bb_socket_t fd = (bb_socket_t)epevents[i].data.fd;

        int idx = _bb_find_fd(poller->fds, poller->count, fd);
        int interest = idx >= 0 ? poller->fds[idx].events : (BB_EVENT_READ | BB_EVENT_WRITE);

        int triggered = _from_epoll_events(epevents[i].events, interest);

        if (triggered)
        {
            events[event_count].fd = fd;
            events[event_count].events = triggered;
            event_count++;
        }
    }

    free(epevents);

    return event_count;
}
