#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <unistd.h>

#include "poller.h"

static void test_poller_rejects_fd_at_or_above_fd_setsize(void)
{
    printf("\tRunning test_poller_rejects_fd_at_or_above_fd_setsize...\n");

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

    // Force a valid, open fd to sit at a specific out-of-range descriptor
    // number without needing to actually open FD_SETSIZE files.
    int high_fd = FD_SETSIZE;
    BB_ASSERT(dup2(pipefd[0], high_fd) == high_fd);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    int rc = bb_poller_register(poller, high_fd, BB_EVENT_READ);

    BB_ASSERT(rc == -1); // fails today: registration silently succeeds

    bb_poller_destroy(poller);

    close(high_fd);
    close(pipefd[0]);
    close(pipefd[1]);
}

int main(void)
{
    printf("Running Poller tests...\n");
    test_poller_rejects_fd_at_or_above_fd_setsize();
    printf("Poller tests passed.\n");
    return 0;
}
