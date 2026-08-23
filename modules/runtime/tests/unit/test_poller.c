#include <blue-bird/error/assert.h>
#include <stdio.h>

#include "poller.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

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

    // Nothing is registered, so this fd is "unknown" regardless of whether
    // it happens to be open -- exercised without touching a real fd since
    // bb_poller_unregister never reaches the kernel on a lookup miss.
    BB_ASSERT(bb_poller_unregister(poller, 3, BB_EVENT_READ) == -1);

    bb_poller_destroy(poller);
}

#if !defined(_WIN32)

// epoll/kqueue validate the fd against the kernel's open-file table right
// at registration time (EBADF if it isn't open), unlike select()'s old
// register() which just stored bookkeeping and never touched the OS until
// wait(). So every test below that expects a successful register() now
// needs a real, open fd -- a bare integer literal is no longer enough.

static void test_poller_register_rejects_invalid_fd(void)
{
    printf("\tRunning test_poller_register_rejects_invalid_fd...\n");

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    // -1 is rejected by the common bb_poller_fd_supported() check before
    // any backend is ever consulted, so this holds regardless of backend.
    //
    // Note: a *closed-but-structurally-valid* fd (e.g. a pipe fd right
    // after close()) is deliberately NOT tested here. epoll/kqueue
    // validate against the kernel immediately at registration and would
    // reject it, but poll()/WSAPoll have no kernel-side registration step
    // at all -- they only discover it's invalid later, at wait() time, as
    // POLLNVAL. That's a real, allowed difference in backend timing, not
    // part of the shared contract, so a test can't assert on it without
    // baking in one specific backend's behavior.
    BB_ASSERT(bb_poller_register(poller, -1, BB_EVENT_READ) == -1);

    bb_poller_destroy(poller);
}

// Historically select()'s fd_set imposed a hard FD_SETSIZE ceiling on the
// numeric fd value; epoll/kqueue/poll have no such ceiling. This confirms
// the fix: a real fd well past the old FD_SETSIZE boundary now registers
// and reports readiness normally.
static void test_poller_supports_fd_above_legacy_fd_setsize(void)
{
    printf("\tRunning test_poller_supports_fd_above_legacy_fd_setsize...\n");

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

    int high_fd = FD_SETSIZE + 16;
    BB_ASSERT(dup2(pipefd[0], high_fd) == high_fd);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    BB_ASSERT(bb_poller_register(poller, high_fd, BB_EVENT_READ) == 0);

    BB_ASSERT(write(pipefd[1], "x", 1) == 1);

    bb_poll_event_t events[4];
    int ready = bb_poller_wait(poller, events, 4, 1000);

    BB_ASSERT(ready == 1);
    BB_ASSERT(events[0].fd == high_fd);
    BB_ASSERT(events[0].events & BB_EVENT_READ);

    bb_poller_destroy(poller);
    close(high_fd);
    close(pipefd[0]);
    close(pipefd[1]);
}

static void test_poller_unregister_partial_events_keeps_fd_registered(void)
{
    printf("\tRunning test_poller_unregister_partial_events_keeps_fd_registered...\n");

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    bb_socket_t fd = pipefd[1]; // write end: supports WRITE; register READ too

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
    close(pipefd[0]);
    close(pipefd[1]);
}

static void test_poller_wait_reports_readable_fd(void)
{
    printf("\tRunning test_poller_wait_reports_readable_fd...\n");

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    BB_ASSERT(bb_poller_register(poller, pipefd[0], BB_EVENT_READ) == 0);

    BB_ASSERT(write(pipefd[1], "x", 1) == 1);

    bb_poll_event_t events[4];
    int ready = bb_poller_wait(poller, events, 4, 1000);

    BB_ASSERT(ready == 1);
    BB_ASSERT(events[0].fd == pipefd[0]);
    BB_ASSERT(events[0].events & BB_EVENT_READ);

    bb_poller_destroy(poller);
    close(pipefd[0]);
    close(pipefd[1]);
}

static void test_poller_wait_times_out_when_nothing_ready(void)
{
    printf("\tRunning test_poller_wait_times_out_when_nothing_ready...\n");

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    // Nothing was ever written, so the read end must not be reported ready.
    BB_ASSERT(bb_poller_register(poller, pipefd[0], BB_EVENT_READ) == 0);

    bb_poll_event_t events[4];
    int ready = bb_poller_wait(poller, events, 4, 50);

    BB_ASSERT(ready == 0);

    bb_poller_destroy(poller);
    close(pipefd[0]);
    close(pipefd[1]);
}

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

#endif // !defined(_WIN32)

int main(void)
{
    printf("Running Poller tests...\n");

    test_poller_null_poller_is_rejected_everywhere();
    test_poller_wait_rejects_null_events_buffer();
    test_poller_unregister_unknown_fd_fails();

#if !defined(_WIN32)
    test_poller_register_rejects_invalid_fd();
    test_poller_supports_fd_above_legacy_fd_setsize();
    test_poller_unregister_partial_events_keeps_fd_registered();
    test_poller_wait_reports_readable_fd();
    test_poller_wait_times_out_when_nothing_ready();
    test_poller_register_merges_repeat_calls_into_one_entry();
#endif

    printf("Poller tests passed.\n");
    return 0;
}
