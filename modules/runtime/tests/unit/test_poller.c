#include <blue-bird/error/assert.h>
#include <stdio.h>

#include "poller.h"

static void test_poller_rejects_fd_at_or_above_fd_setsize(void)
{
    printf("\tRunning test_poller_rejects_fd_at_or_above_fd_setsize...\n");

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    BB_ASSERT(bb_poller_register(poller, FD_SETSIZE - 1, BB_EVENT_READ) == 0);
    BB_ASSERT(bb_poller_register(poller, FD_SETSIZE,     BB_EVENT_READ) == -1);
    BB_ASSERT(bb_poller_register(poller, FD_SETSIZE + 1, BB_EVENT_READ) == -1);

    bb_poller_destroy(poller);
}

int main(void)
{
    printf("Running Poller tests...\n");
    test_poller_rejects_fd_at_or_above_fd_setsize();
    printf("Poller tests passed.\n");
    return 0;
}
