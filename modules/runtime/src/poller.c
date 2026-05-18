#include <stdlib.h>

#include "blue-bird/runtime/poller.h"

struct bb_poller {
    int reserved;
};

bb_poller_t *bb_poller_create(void)
{
    bb_poller_t *poller = malloc(sizeof(bb_poller_t));

    if (!poller)
    {
        return NULL;
    }

    poller->reserved = 0;

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

void bb_poller_poll(bb_poller_t *poller)
{
    (void)poller;

    /*
     * Stub for future epoll/select/kqueue
     */
}
