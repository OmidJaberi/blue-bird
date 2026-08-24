#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "poller_backend.h"

int _bb_poller_backend_create(bb_poller_t *poller)
{
    poller->kq = kqueue();

    return poller->kq >= 0 ? 0 : -1;
}

void _bb_poller_backend_destroy(bb_poller_t *poller)
{
    close(poller->kq);
}

int _bb_poller_backend_register(bb_poller_t *poller, bb_socket_t fd, int events)
{
    int idx = _bb_find_fd(poller->fds, poller->count, fd);
    int existing = idx >= 0 ? poller->fds[idx].events : 0;

    /* kqueue registers READ/WRITE as independent filters (unlike epoll's
     * single combined interest mask), so only the genuinely new sides
     * need an EV_ADD -- re-adding an already-active filter is harmless
     * but wasteful, and unlike epoll there's no single "MOD" call. */
    int to_add = events & ~existing;

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

    return 0;
}

int _bb_poller_backend_unregister(bb_poller_t *poller, bb_socket_t fd, int events)
{
    int idx = _bb_find_fd(poller->fds, poller->count, fd);

    if (idx < 0)
    {
        return -1;
    }

    int to_remove = poller->fds[idx].events & events;

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
         * kernel already dropped its kevents. */
        kevent(poller->kq, changes, nchanges, NULL, 0, NULL);
    }

    return 0;
}

int _bb_poller_backend_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
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
     * for the same fd, unlike epoll/poll's combined bitmask, so a fd
     * that's ready both ways can appear twice in evlist. Coalesce those
     * into one bb_poll_event_t per fd to match the shared contract. */
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
