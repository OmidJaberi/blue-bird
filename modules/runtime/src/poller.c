#include <stdlib.h>
#include <string.h>

#include "poller.h"

#define BB_POLLER_MAX_FDS 1024

/* ======================================================================= */
/* Shared bookkeeping                                                      */
/*                                                                         */
/* All three backends keep the same fd -> requested-events table. It's the */
/* source of truth for "what is fd X currently registered for", which we   */
/* need in order to (a) support merge-on-repeat-register / partial-        */
/* unregister semantics, and (b) tell the OS the right thing to add/modify */
/* /delete, since epoll and kqueue are edge-registration APIs rather than  */
/* select()'s "just pass the whole set every call" model.                  */
/* ======================================================================= */

typedef struct {
    bb_socket_t fd;
    int events; // BB_EVENT_* bitmask currently registered for this fd
} _bb_poll_fd_t;

static int _bb_find_fd(_bb_poll_fd_t *fds, int count, bb_socket_t fd)
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
/* epoll backend (Linux)                                                   */
/* ======================================================================= */
#if defined(BB_POLLER_BACKEND_EPOLL)

#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

struct bb_poller {
    int epfd;
    _bb_poll_fd_t fds[BB_POLLER_MAX_FDS];
    int count;
};

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

bb_poller_t *bb_poller_create(void)
{
    bb_poller_t *poller = calloc(1, sizeof(bb_poller_t));

    if (!poller)
    {
        return NULL;
    }

    poller->epfd = epoll_create1(0);

    if (poller->epfd < 0)
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

    close(poller->epfd);
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

    if (idx >= 0)
    {
        int new_events = poller->fds[idx].events | events;

        if (new_events != poller->fds[idx].events)
        {
            struct epoll_event ev = {0};
            ev.events = _to_epoll_events(new_events);
            ev.data.fd = fd;

            if (epoll_ctl(poller->epfd, EPOLL_CTL_MOD, fd, &ev) != 0)
            {
                return -1;
            }
        }

        poller->fds[idx].events = new_events;
        return 0;
    }

    if (poller->count >= BB_POLLER_MAX_FDS)
    {
        return -1;
    }

    struct epoll_event ev = {0};
    ev.events = _to_epoll_events(events);
    ev.data.fd = fd;

    if (epoll_ctl(poller->epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        return -1;
    }

    poller->fds[poller->count].fd = fd;
    poller->fds[poller->count].events = events;
    poller->count++;

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

    int new_events = poller->fds[idx].events & ~events;

    if (new_events == 0)
    {
        /* Kernel requires a non-NULL event pointer on some older kernels
         * even for EPOLL_CTL_DEL; harmless to pass one. */
        struct epoll_event ev = {0};
        epoll_ctl(poller->epfd, EPOLL_CTL_DEL, fd, &ev);

        poller->fds[idx] = poller->fds[poller->count - 1];
        poller->count--;
    }
    else if (new_events != poller->fds[idx].events)
    {
        struct epoll_event ev = {0};
        ev.events = _to_epoll_events(new_events);
        ev.data.fd = fd;

        epoll_ctl(poller->epfd, EPOLL_CTL_MOD, fd, &ev);

        poller->fds[idx].events = new_events;
    }

    return 0;
}

int bb_poller_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
    if (!poller || !events)
    {
        return -1;
    }

    struct epoll_event epevents[BB_POLLER_MAX_FDS];
    int cap = max_events < (int)(sizeof(epevents) / sizeof(epevents[0]))
                  ? max_events
                  : (int)(sizeof(epevents) / sizeof(epevents[0]));

    int n;
    do
    {
        n = epoll_wait(poller->epfd, epevents, cap, timeout_ms);
    } while (n < 0 && errno == EINTR);

    if (n <= 0)
    {
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

    return event_count;
}

#endif /* BB_POLLER_BACKEND_EPOLL */

/* ======================================================================= */
/* kqueue backend (macOS / BSD)                                            */
/* ======================================================================= */
#if defined(BB_POLLER_BACKEND_KQUEUE)

#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

struct bb_poller {
    int kq;
    _bb_poll_fd_t fds[BB_POLLER_MAX_FDS];
    int count;
};

bb_poller_t *bb_poller_create(void)
{
    bb_poller_t *poller = calloc(1, sizeof(bb_poller_t));

    if (!poller)
    {
        return NULL;
    }

    poller->kq = kqueue();

    if (poller->kq < 0)
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

    close(poller->kq);
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
    int existing = idx >= 0 ? poller->fds[idx].events : 0;
    int to_add = events & ~existing;

    if (idx < 0 && poller->count >= BB_POLLER_MAX_FDS)
    {
        return -1;
    }

    struct kevent changes[2];
    int nchanges = 0;

    if (to_add & BB_EVENT_READ)
    {
        EV_SET(&changes[nchanges++], fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    }

    if (to_add & BB_EVENT_WRITE)
    {
        EV_SET(&changes[nchanges++], fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
    }

    if (nchanges > 0 && kevent(poller->kq, changes, nchanges, NULL, 0, NULL) != 0)
    {
        return -1;
    }

    if (idx >= 0)
    {
        poller->fds[idx].events = existing | events;
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

    int existing = poller->fds[idx].events;
    int to_remove = existing & events;

    struct kevent changes[2];
    int nchanges = 0;

    if (to_remove & BB_EVENT_READ)
    {
        EV_SET(&changes[nchanges++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    }

    if (to_remove & BB_EVENT_WRITE)
    {
        EV_SET(&changes[nchanges++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    }

    if (nchanges > 0)
    {
        /* Best-effort: the fd may already be closed, in which case the
         * kernel has already dropped its kevents; ignore errors here the
         * same way the old select()-based unregister never checked. */
        kevent(poller->kq, changes, nchanges, NULL, 0, NULL);
    }

    int new_events = existing & ~events;

    if (new_events == 0)
    {
        poller->fds[idx] = poller->fds[poller->count - 1];
        poller->count--;
    }
    else
    {
        poller->fds[idx].events = new_events;
    }

    return 0;
}

int bb_poller_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
    if (!poller || !events)
    {
        return -1;
    }

    struct kevent evlist[BB_POLLER_MAX_FDS];
    int cap = max_events < (int)(sizeof(evlist) / sizeof(evlist[0]))
                  ? max_events
                  : (int)(sizeof(evlist) / sizeof(evlist[0]));

    struct timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;

    int n;
    do
    {
        n = kevent(poller->kq, NULL, 0, evlist, cap, &ts);
    } while (n < 0 && errno == EINTR);

    if (n <= 0)
    {
        return n;
    }

    /* kqueue delivers READ and WRITE readiness as separate kevents even
     * for the same fd, unlike epoll/select's combined bitmask, so a fd
     * that's ready both ways can appear twice in evlist. Coalesce those
     * into one bb_poll_event_t per fd to match the existing contract. */
    bb_poll_event_t coalesced[BB_POLLER_MAX_FDS];
    int event_count = 0;

    for (int i = 0; i < n; i++)
    {
        bb_socket_t fd = (bb_socket_t)evlist[i].ident;

        int triggered = 0;

        if (evlist[i].filter == EVFILT_READ)  triggered = BB_EVENT_READ;
        if (evlist[i].filter == EVFILT_WRITE) triggered = BB_EVENT_WRITE;

        if (evlist[i].flags & (EV_EOF | EV_ERROR))
        {
            triggered |= (BB_EVENT_READ | BB_EVENT_WRITE);
        }

        int out_idx = -1;

        for (int j = 0; j < event_count; j++)
        {
            if (coalesced[j].fd == fd)
            {
                out_idx = j;
                break;
            }
        }

        if (out_idx < 0 && event_count < max_events)
        {
            out_idx = event_count++;
            coalesced[out_idx].fd = fd;
            coalesced[out_idx].events = 0;
        }

        if (out_idx >= 0)
        {
            coalesced[out_idx].events |= triggered;
        }
    }

    memcpy(events, coalesced, (size_t)event_count * sizeof(bb_poll_event_t));

    return event_count;
}

#endif /* BB_POLLER_BACKEND_KQUEUE */

/* ======================================================================= */
/* poll()/WSAPoll backend (Windows, and generic POSIX fallback)            */
/* ======================================================================= */
#if defined(BB_POLLER_BACKEND_POLL)

#if defined(_WIN32)
typedef WSAPOLLFD _bb_pollfd_t;
#define _bb_poll_fn(fds, nfds, timeout) WSAPoll((fds), (nfds), (timeout))
#else
#include <poll.h>
typedef struct pollfd _bb_pollfd_t;
#include <errno.h>
#define _bb_poll_fn(fds, nfds, timeout) poll((fds), (nfds), (timeout))
#endif

struct bb_poller {
    _bb_poll_fd_t fds[BB_POLLER_MAX_FDS];
    int count;
};

bb_poller_t *bb_poller_create(void)
{
    return calloc(1, sizeof(bb_poller_t));
}

void bb_poller_destroy(bb_poller_t *poller)
{
    if (!poller)
    {
        return;
    }

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

    if (idx >= 0)
    {
        poller->fds[idx].events |= events;
        return 0;
    }

    if (poller->count >= BB_POLLER_MAX_FDS)
    {
        return -1;
    }

    poller->fds[poller->count].fd = fd;
    poller->fds[poller->count].events = events;
    poller->count++;

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

    _bb_pollfd_t pfds[BB_POLLER_MAX_FDS];

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

    return event_count;
}

#endif /* BB_POLLER_BACKEND_POLL */
