#ifndef BB_POLLER_INTERNAL_H
#define BB_POLLER_INTERNAL_H

#include "poller.h"

#define BB_POLLER_MAX_FDS 1024

/*
 * fd -> requested BB_EVENT_* mask. This is the single source of truth for
 * "what is fd X currently registered for"; every backend reads it (via
 * _bb_find_fd) to figure out what changed, rather than keeping its own
 * shadow copy.
 */
typedef struct {
    bb_socket_t fd;
    int events;
} _bb_poll_fd_t;

struct bb_poller {
    _bb_poll_fd_t fds[BB_POLLER_MAX_FDS];
    int count;

/*
 * OS-specific state, selected at compile time by whichever single
 * BB_POLLER_BACKEND_* macro is defined (see blue-bird/utils/platform.h).
 * Exactly one of src/poller_epoll.c, src/poller_kqueue.c, or
 * src/poller_poll.c is compiled alongside this struct definition, so
 * there is never a mismatch between the field present here and the
 * backend that uses it.
 */
#if defined(BB_POLLER_BACKEND_EPOLL)
    int epfd;
#elif defined(BB_POLLER_BACKEND_KQUEUE)
    int kq;
#endif
    /*
     * BB_POLLER_BACKEND_POLL needs no persistent OS-level state: poll()/
     * WSAPoll() take an ephemeral array built fresh from fds[] on every
     * bb_poller_wait() call, so there's nothing to store here for it.
     */
};

/* Shared lookup helper. Implemented once in poller.c; used by poller.c
 * itself and by every backend file to look up an fd's pre-update mask. */
int _bb_find_fd(_bb_poll_fd_t *fds, int count, bb_socket_t fd);

/* ----------------------------------------------------------------------
 * Backend interface.
 *
 * Implemented exactly once, by whichever of poller_epoll.c /
 * poller_kqueue.c / poller_poll.c is compiled for the current platform
 * (selected in CMakeLists.txt, matching BB_POLLER_BACKEND_* in
 * blue-bird/utils/platform.h). poller.c owns all validation and fds[]
 * bookkeeping and calls these purely to perform the OS-level operation.
 * ---------------------------------------------------------------------- */

/* Create/destroy the OS-level poller handle (poller->epfd / poller->kq /
 * nothing, depending on backend). Returns 0 on success, -1 on failure. */
int _bb_poller_backend_create(bb_poller_t *poller);
void _bb_poller_backend_destroy(bb_poller_t *poller);

/*
 * `events` is the delta the caller asked bb_poller_register() to add.
 * poller->fds[] still holds fd's pre-update mask at the time this is
 * called (poller.c only merges the bits into the table after this
 * returns success), so the backend can look up the old mask via
 * _bb_find_fd() to decide e.g. epoll ADD vs MOD, or which kqueue filters
 * are genuinely new. Returns 0 on success, -1 on failure (in which case
 * poller.c leaves its bookkeeping untouched).
 */
int _bb_poller_backend_register(bb_poller_t *poller, bb_socket_t fd, int events);

/*
 * `events` is the delta the caller asked bb_poller_unregister() to
 * remove; same pre-update visibility into poller->fds[] as register().
 * Best-effort: the fd may already be closed by the time this runs, so
 * backends should not fail this call (matches the historical behavior
 * of never checking the underlying removal syscall's return value).
 */
int _bb_poller_backend_unregister(bb_poller_t *poller, bb_socket_t fd, int events);

/* Waits for events and fills `events` (backend-owned translation to
 * BB_EVENT_READ/WRITE, including any backend-specific coalescing).
 * Returns the number of ready entries written (0 on timeout, -1 on
 * error), same contract as the public bb_poller_wait(). */
int _bb_poller_backend_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms);

#endif
