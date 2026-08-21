#include <blue-bird/error/assert.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "poller.h"

static void test_poller_rejects_fd_at_or_above_fd_setsize(void)
{
    printf("\tRunning test_poller_rejects_fd_at_or_above_fd_setsize...\n");

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    BB_ASSERT(bb_poller_register(poller, FD_SETSIZE - 1, BB_EVENT_READ) == 0);
#if !defined(_WIN32)
    BB_ASSERT(bb_poller_register(poller, FD_SETSIZE,     BB_EVENT_READ) == -1);
    BB_ASSERT(bb_poller_register(poller, FD_SETSIZE + 1, BB_EVENT_READ) == -1);
#endif

    bb_poller_destroy(poller);
}

static void test_poller_null_poller_is_rejected_everywhere(void)
{
    printf("\tRunning test_poller_null_poller_is_rejected_everywhere...\n");

    bb_poll_event_t events[4];

    BB_ASSERT(bb_poller_register(NULL, 0, BB_EVENT_READ) == -1);
    BB_ASSERT(bb_poller_unregister(NULL, 0, BB_EVENT_READ) == -1);
    BB_ASSERT(bb_poller_wait(NULL, events, 4, 0) == -1);

    // Must not crash.
    bb_poller_destroy(NULL);
}

static void test_poller_wait_rejects_null_events_buffer(void)
{
    printf("\tRunning test_poller_wait_rejects_null_events_buffer...\n");

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    BB_ASSERT(bb_poller_wait(poller, NULL, 4, 0) == -1);

    bb_poller_destroy(poller);
}

static void test_poller_unregister_unknown_fd_fails(void)
{
    printf("\tRunning test_poller_unregister_unknown_fd_fails...\n");

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    BB_ASSERT(bb_poller_unregister(poller, 3, BB_EVENT_READ) == -1);

    bb_poller_destroy(poller);
}

static void test_poller_unregister_partial_events_keeps_fd_registered(void)
{
    printf("\tRunning test_poller_unregister_partial_events_keeps_fd_registered...\n");

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    bb_socket_t fd = 3;

    BB_ASSERT(bb_poller_register(poller, fd, BB_EVENT_READ | BB_EVENT_WRITE) == 0);

    // Removing just READ should succeed and leave the fd registered for WRITE.
    BB_ASSERT(bb_poller_unregister(poller, fd, BB_EVENT_READ) == 0);

    // Removing READ again is a no-op from the caller's perspective, but the
    // fd is still tracked (for WRITE), so this must still succeed.
    BB_ASSERT(bb_poller_unregister(poller, fd, BB_EVENT_READ) == 0);

    // Removing WRITE now fully clears the fd's entry.
    BB_ASSERT(bb_poller_unregister(poller, fd, BB_EVENT_WRITE) == 0);

    // The fd is no longer tracked at all -- further unregister calls fail.
    BB_ASSERT(bb_poller_unregister(poller, fd, BB_EVENT_WRITE) == -1);

    bb_poller_destroy(poller);
}

#if !defined(_WIN32)

static void test_poller_register_merges_repeat_calls_into_one_entry(void)
{
    printf("\tRunning test_poller_register_merges_repeat_calls_into_one_entry...\n");

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    // Registering the same fd for the same event twice must merge into a
    // single tracked entry, not create a duplicate. If it didn't merge,
    // bb_poller_wait would report this one ready fd twice.
    BB_ASSERT(bb_poller_register(poller, pipefd[1], BB_EVENT_WRITE) == 0);
    BB_ASSERT(bb_poller_register(poller, pipefd[1], BB_EVENT_WRITE) == 0);

    bb_poll_event_t events[4];
    int ready = bb_poller_wait(poller, events, 4, 1000);

    BB_ASSERT(ready == 1); // <-- would be 2 if register() duplicated the entry
    BB_ASSERT(events[0].fd == pipefd[1]);
    BB_ASSERT(events[0].events & BB_EVENT_WRITE);

    bb_poller_destroy(poller);
    close(pipefd[0]);
    close(pipefd[1]);
}

#endif

int main(void)
{
    printf("Running Poller tests...\n");
    test_poller_rejects_fd_at_or_above_fd_setsize();
    test_poller_null_poller_is_rejected_everywhere();
    test_poller_wait_rejects_null_events_buffer();
    test_poller_unregister_unknown_fd_fails();
    test_poller_unregister_partial_events_keeps_fd_registered();
#if !defined(_WIN32)
    test_poller_register_merges_repeat_calls_into_one_entry();
#endif
    printf("Poller tests passed.\n");
    return 0;
}
