#include <blue-bird/error/assert.h>
#include <stdio.h>

#include "poller.h"

#if !defined(_WIN32)
#include <unistd.h>
#include <sys/resource.h>
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

// dup2()-ing a real fd up to FD_SETSIZE + 16 can exceed the process's
// default open-file *soft* limit (e.g. macOS defaults RLIMIT_NOFILE to
// 256, right around FD_SETSIZE itself), which would fail the dup2() below
// for reasons that have nothing to do with the poller under test. Raise
// the soft limit far enough to fit `needed`, capped by whatever the hard
// limit allows, and hand back the previous limit so the caller can
// restore it -- we don't want to leave process-wide rlimit state changed
// for tests that run after this one.
//
// Returns 0 if `needed` fds are available (whether or not a raise was
// necessary), -1 if the hard limit won't allow it.
static int _ensure_fd_limit(int needed, struct rlimit *previous)
{
    if (getrlimit(RLIMIT_NOFILE, previous) != 0)
    {
        return -1;
    }

    if (previous->rlim_cur > (rlim_t)needed)
    {
        return 0; // already enough headroom, nothing to do
    }

    if (previous->rlim_max != RLIM_INFINITY && previous->rlim_max <= (rlim_t)needed)
    {
        return -1; // hard limit won't let us get there
    }

    struct rlimit raised = *previous;
    raised.rlim_cur = (rlim_t)needed + 1;

    return setrlimit(RLIMIT_NOFILE, &raised);
}

// Historically select()'s fd_set imposed a hard FD_SETSIZE ceiling on the
// numeric fd value; epoll/kqueue/poll have no such ceiling. This confirms
// the fix: a real fd well past the old FD_SETSIZE boundary now registers
// and reports readiness normally.
static void test_poller_supports_fd_above_legacy_fd_setsize(void)
{
    printf("\tRunning test_poller_supports_fd_above_legacy_fd_setsize...\n");

    int high_fd = FD_SETSIZE + 16;

    struct rlimit previous_limit;
    if (_ensure_fd_limit(high_fd, &previous_limit) != 0)
    {
        printf("\t\tskipped: RLIMIT_NOFILE hard limit is too low to open fd %d\n", high_fd);
        return;
    }

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

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

    setrlimit(RLIMIT_NOFILE, &previous_limit); // best-effort restore
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

// ------------------------------------------------------------------------
// Socket exhaustion (EMFILE/ENFILE) handling
//
// EMFILE ("process fd table full") and ENFILE ("system-wide fd table
// full") both surface to us identically: whatever syscall would have
// handed back a new fd instead returns -1. The most portable way to
// force that deterministically in a unit test is EMFILE via
// RLIMIT_NOFILE, since it doesn't require actually driving the whole
// system out of descriptors -- but the code path exercised inside
// epoll_create1()/kqueue() returning -1 is the same one ENFILE takes.
//
// Pinning rlim_cur at 3 works regardless of how many fds happen to
// already be open: fds 0/1/2 (stdio) are always taken, so the *next*
// fd the kernel would hand out is always >= 3 -- i.e. >= rlim_cur --
// which is exactly what makes open()/socket()/epoll_create1()/kqueue()
// fail with EMFILE. Existing, already-open fds are completely
// unaffected; only *new* fd creation is blocked.
static int _exhaust_fd_table(struct rlimit *previous)
{
    if (getrlimit(RLIMIT_NOFILE, previous) != 0)
    {
        return -1;
    }

    struct rlimit exhausted = *previous;
    exhausted.rlim_cur = 3;

    return setrlimit(RLIMIT_NOFILE, &exhausted);
}

#if defined(BB_POLLER_BACKEND_EPOLL) || defined(BB_POLLER_BACKEND_KQUEUE)

// epoll_create1()/kqueue() each need to hand back a brand-new kernel fd.
// With the fd table pinned full, that allocation itself fails, so
// bb_poller_create() must report failure cleanly (NULL) instead of
// handing back a poller wrapping a bogus/negative epfd or kq that later
// calls would blindly pass to epoll_ctl()/kevent() and crash on.
static void test_poller_create_fails_gracefully_under_fd_exhaustion(void)
{
    printf("\tRunning test_poller_create_fails_gracefully_under_fd_exhaustion...\n");

    struct rlimit previous_limit;
    BB_ASSERT(_exhaust_fd_table(&previous_limit) == 0);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller == NULL);

    // Must also be safe to hand a NULL straight to destroy after a
    // failed create, same as any other caller error path.
    bb_poller_destroy(poller);

    setrlimit(RLIMIT_NOFILE, &previous_limit); // best-effort restore
}

#endif // BB_POLLER_BACKEND_EPOLL || BB_POLLER_BACKEND_KQUEUE

#if defined(BB_POLLER_BACKEND_POLL)

// Unlike epoll/kqueue, poll()/WSAPoll() need no persistent kernel-side
// handle -- _bb_poller_backend_create() is a pure no-op. So a fully
// exhausted fd table must NOT stop a poller from being created here;
// this is the one backend that keeps degrading gracefully rather than
// refusing outright when the process is completely out of descriptors.
static void test_poller_create_succeeds_without_kernel_fd_under_exhaustion(void)
{
    printf("\tRunning test_poller_create_succeeds_without_kernel_fd_under_exhaustion...\n");

    struct rlimit previous_limit;
    BB_ASSERT(_exhaust_fd_table(&previous_limit) == 0);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    setrlimit(RLIMIT_NOFILE, &previous_limit); // best-effort restore

    bb_poller_destroy(poller);
}

#endif // BB_POLLER_BACKEND_POLL

// The realistic production scenario: a long-running server hits
// EMFILE/ENFILE while trying to accept() new connections. New
// connections get rejected elsewhere (bb_connection_accept() already
// returns NULL uniformly for any accept() failure), but the event loop
// itself -- and every connection it was already serving before the fd
// table filled up -- must keep working. None of register/unregister/
// wait need to allocate a *new* kernel fd for an fd the backend already
// knows about (EPOLL_CTL_MOD, a repeat kevent EV_ADD, and rebuilding the
// ephemeral poll() array all reuse existing handles), so none of them
// should start failing just because the process is out of descriptors.
static void test_poller_register_and_wait_survive_fd_table_exhaustion(void)
{
    printf("\tRunning test_poller_register_and_wait_survive_fd_table_exhaustion...\n");

    int pipefd[2];
    BB_ASSERT(pipe(pipefd) == 0);

    bb_poller_t *poller = bb_poller_create();
    BB_ASSERT(poller != NULL);

    BB_ASSERT(bb_poller_register(poller, pipefd[0], BB_EVENT_READ) == 0);

    struct rlimit previous_limit;
    BB_ASSERT(_exhaust_fd_table(&previous_limit) == 0);

    // Toggle the fd's registration while the fd table is completely
    // full: unregister must still succeed (it's best-effort/bookkeeping
    // only), and re-registering it must still succeed too, since it
    // touches only the already-open pipe fd and the already-open
    // epfd/kq -- no new fd is created by either call.
    BB_ASSERT(bb_poller_unregister(poller, pipefd[0], BB_EVENT_READ) == 0);
    BB_ASSERT(bb_poller_register(poller, pipefd[0], BB_EVENT_READ) == 0);

    BB_ASSERT(write(pipefd[1], "x", 1) == 1);

    bb_poll_event_t events[4];
    int ready = bb_poller_wait(poller, events, 4, 1000);

    BB_ASSERT(ready == 1);
    BB_ASSERT(events[0].fd == pipefd[0]);
    BB_ASSERT(events[0].events & BB_EVENT_READ);

    setrlimit(RLIMIT_NOFILE, &previous_limit); // restore before teardown

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

#if defined(BB_POLLER_BACKEND_EPOLL) || defined(BB_POLLER_BACKEND_KQUEUE)
    test_poller_create_fails_gracefully_under_fd_exhaustion();
#endif
#if defined(BB_POLLER_BACKEND_POLL)
    test_poller_create_succeeds_without_kernel_fd_under_exhaustion();
#endif
    test_poller_register_and_wait_survive_fd_table_exhaustion();
#endif

    printf("Poller tests passed.\n");
    return 0;
}
