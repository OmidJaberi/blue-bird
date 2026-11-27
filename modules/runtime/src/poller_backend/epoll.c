#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
    poller->scan_fd = -1;

    return poller->epfd >= 0 ? 0 : -1;
}

void _bb_poller_backend_destroy(bb_poller_t *poller)
{
    if (!poller)
    {
        return;
    }
    close(poller->epfd);
    free(poller->ep_scratch);
    free(poller->ready_scratch);
}

int _bb_poller_backend_register(bb_poller_t *poller, bb_socket_t fd, int events)
{
    int idx = _bb_poller_find(poller, fd);
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
    int idx = _bb_poller_find(poller, fd);

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

static int _cmp_event_fd(const void *a, const void *b)
{
    const bb_poll_event_t *x = a;
    const bb_poll_event_t *y = b;

    return (x->fd > y->fd) - (x->fd < y->fd);
}

int _bb_poller_backend_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
    /*
     * Ask the kernel for every ready fd, not just max_events of them, so
     * fairness doesn't depend on how a particular kernel orders (or
     * fails to rotate) its ready list. Real Linux re-queues
     * level-triggered fds at the tail after reporting them, but that is
     * an implementation detail: emulated or minimal epoll
     * implementations may report the same first N fds on every call,
     * which starves the rest whenever more than max_events watchers stay
     * readable across ticks.
     */
    int cap = poller->count > max_events ? poller->count : max_events;

    /* The buffers live as long as the poller: with many descriptors they are
     * large enough to be mmap()ed and unmapped by the allocator, and doing that
     * on every tick costs more than the wait itself. */
    if (cap > poller->scratch_capacity)
    {
        struct epoll_event *grown_ep = realloc(poller->ep_scratch, (size_t)cap * sizeof(*grown_ep));
        if (grown_ep)
        {
            poller->ep_scratch = grown_ep;
        }

        bb_poll_event_t *grown_ready = realloc(poller->ready_scratch, (size_t)cap * sizeof(*grown_ready));
        if (grown_ready)
        {
            poller->ready_scratch = grown_ready;
        }

        if (!grown_ep || !grown_ready)
        {
            /* Whatever did grow is kept; but only a pair that both fit may be used. */
            return -1;
        }

        poller->scratch_capacity = cap;
    }

    struct epoll_event *epevents = poller->ep_scratch;
    bb_poll_event_t *ready = poller->ready_scratch;

    int n;
    do
    {
        n = epoll_wait(poller->epfd, epevents, cap, timeout_ms);
    } while (n < 0 && errno == EINTR);

    if (n <= 0)
    {
        return n;
    }

    int ready_count = 0;

    for (int i = 0; i < n; i++)
    {
        bb_socket_t fd = (bb_socket_t)epevents[i].data.fd;

        int idx = _bb_poller_find(poller, fd);
        int interest = idx >= 0 ? poller->fds[idx].events : (BB_EVENT_READ | BB_EVENT_WRITE);

        int triggered = _from_epoll_events(epevents[i].events, interest);

        if (triggered)
        {
            ready[ready_count].fd = fd;
            ready[ready_count].events = triggered;
            ready_count++;
        }
    }

    int event_count;

    if (ready_count <= max_events)
    {
        // Everything fits: nothing can be starved, report it all.
        memcpy(events, ready, (size_t)ready_count * sizeof(*ready));
        event_count = ready_count;
    }
    else
    {
        // More ready than we can report. Order by fd and resume just
        // after the last fd reported previously, wrapping around, so
        // every ready fd is reached within a bounded number of calls.
        qsort(ready, (size_t)ready_count, sizeof(*ready), _cmp_event_fd);

        int start = 0;

        while (start < ready_count && ready[start].fd <= poller->scan_fd)
        {
            start++;
        }

        for (int k = 0; k < max_events; k++)
        {
            events[k] = ready[(start + k) % ready_count];
        }

        event_count = max_events;
        poller->scan_fd = events[max_events - 1].fd;
    }

    return event_count;
}
