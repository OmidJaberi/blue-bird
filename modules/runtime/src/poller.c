#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "poller.h"

#define BB_POLLER_MAX_FDS 1024

typedef struct {
    int fd;
    int events;
} _bb_poll_fd_t;

struct bb_poller {
    _bb_poll_fd_t fds[BB_POLLER_MAX_FDS];
    int count;
};

bb_poller_t *bb_poller_create(void)
{
    bb_poller_t *poller = malloc(sizeof(bb_poller_t));

    if (!poller)
    {
        return NULL;
    }

    memset(poller, 0, sizeof(*poller));

    return poller;
}

void bb_poller_destroy(bb_poller_t *poller)
{
    if (!poller)
    {
        return;
    }

    free(poller);
}

int bb_poller_register(bb_poller_t *poller, int fd, int events)
{
    if (!poller)
    {
        return -1;
    }

    for (int i = 0; i < poller->count; i++)
    {
        if (poller->fds[i].fd == fd)
        {
            poller->fds[i].events |= events;
            return 0;
        }
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

int bb_poller_unregister(bb_poller_t *poller, int fd, int events)
{
    if (!poller)
    {
        return -1;
    }

    for (int i = 0; i < poller->count; i++)
    {
        if (poller->fds[i].fd == fd)
        {
            poller->fds[i].events &= ~events;

            if (poller->fds[i].events == 0)
            {
                poller->fds[i] = poller->fds[poller->count - 1];
                poller->count--;
            }

            return 0;
        }
    }

    return -1;
}

int bb_poller_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms)
{
    if (!poller || !events)
    {
        return -1;
    }

    fd_set readfds;
    fd_set writefds;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    int maxfd = 0;

    for (int i = 0; i < poller->count; i++)
    {
        int fd = poller->fds[i].fd;

        if (poller->fds[i].events & BB_EVENT_READ)
        {
            FD_SET(fd, &readfds);
        }

        if (poller->fds[i].events & BB_EVENT_WRITE)
        {
            FD_SET(fd, &writefds);
        }

        if (fd > maxfd)
        {
            maxfd = fd;
        }
    }

    struct timeval tv;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select(
        maxfd + 1,
        &readfds,
        &writefds,
        NULL,
        &tv
    );

    if (ready <= 0)
    {
        return ready;
    }

    int event_count = 0;

    for (int i = 0; i < poller->count && event_count < max_events; i++)
    {

        int triggered = 0;

        int fd = poller->fds[i].fd;

        if (FD_ISSET(fd, &readfds))
        {
            triggered |= BB_EVENT_READ;
        }

        if (FD_ISSET(fd, &writefds))
        {
            triggered |= BB_EVENT_WRITE;
        }

        if (triggered)
        {
            events[event_count].fd = fd;
            events[event_count].events = triggered;

            event_count++;
        }
    }

    return event_count;
}
