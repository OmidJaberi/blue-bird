#include <stdint.h>
#include <stdlib.h>

#include "poller_backend.h"

/* ======================================================================= */
/* fd index                                                                */
/*                                                                         */
/* fds[] stays dense (the poll() backend walks it), so finding an fd by    */
/* value would be a scan. This hash index, chained through                 */
/* _bb_poll_fd_t::hnext, answers it in O(1) on average. The chains hold    */
/* indices, so they survive fds[] being reallocated; the only moves are    */
/* the swap-removal in unregister, which patches them.                     */
/* ======================================================================= */

static inline unsigned _bb_poller_bucket(const bb_poller_t *poller, bb_socket_t fd)
{
    /* Multiplicative (Fibonacci) hashing: spreads both the small consecutive
     * descriptors of POSIX and the large, patterned SOCKET values of Windows. */
    uint64_t h = (uint64_t)(uintptr_t)fd * 0x9E3779B97F4A7C15ull;
    return (unsigned)(h >> 32) & (poller->hbucket_count - 1);
}

int _bb_poller_find(const bb_poller_t *poller, bb_socket_t fd)
{
    for (int i = poller->hbuckets[_bb_poller_bucket(poller, fd)]; i >= 0; i = poller->fds[i].hnext)
    {
        if (poller->fds[i].fd == fd)
        {
            return i;
        }
    }

    return -1;
}

static void _bb_poller_hash_insert(bb_poller_t *poller, int index)
{
    unsigned bucket = _bb_poller_bucket(poller, poller->fds[index].fd);

    poller->fds[index].hnext = poller->hbuckets[bucket];
    poller->hbuckets[bucket] = index;
}

/* Rewrites the link that currently points at `index` to point at `to` instead
 * (-1 to just drop it). The entry must be in the index. */
static void _bb_poller_hash_relink(bb_poller_t *poller, int index, int to)
{
    int *link = &poller->hbuckets[_bb_poller_bucket(poller, poller->fds[index].fd)];

    while (*link != index)
    {
        link = &poller->fds[*link].hnext;
    }

    *link = to;
}

/* Doubles the buckets once there are more entries than buckets, keeping chains
 * short. Best effort: if the memory is not available the old, merely more
 * crowded, index keeps working. */
static void _bb_poller_hash_grow(bb_poller_t *poller)
{
    unsigned new_count = poller->hbucket_count * 2;
    int *grown = malloc((size_t)new_count * sizeof(*grown));

    if (!grown)
    {
        return;
    }

    for (unsigned b = 0; b < new_count; b++)
    {
        grown[b] = -1;
    }

    free(poller->hbuckets);
    poller->hbuckets = grown;
    poller->hbucket_count = new_count;

    for (int i = 0; i < poller->count; i++)
    {
        _bb_poller_hash_insert(poller, i);
    }
}

/* ======================================================================= */
/* Public API -- all common semantics/bookkeeping live here. Every backend */
/* only performs the actual OS-level operation and reports success/       */
/* failure; it never touches poller->fds[]/poller->count directly.        */
/* ======================================================================= */

bb_poller_t *bb_poller_create(void)
{
    bb_poller_t *poller = calloc(1, sizeof(bb_poller_t));

    if (!poller)
    {
        return NULL;
    }

    poller->fds = malloc(BB_POLLER_INITIAL_CAPACITY * sizeof(*poller->fds));

    if (!poller->fds)
    {
        free(poller);
        return NULL;
    }

    poller->capacity = BB_POLLER_INITIAL_CAPACITY;

    poller->hbuckets = malloc(BB_POLLER_INITIAL_BUCKETS * sizeof(*poller->hbuckets));

    if (!poller->hbuckets)
    {
        free(poller->fds);
        free(poller);
        return NULL;
    }

    poller->hbucket_count = BB_POLLER_INITIAL_BUCKETS;
    for (unsigned b = 0; b < poller->hbucket_count; b++)
    {
        poller->hbuckets[b] = -1;
    }

    if (_bb_poller_backend_create(poller) != 0)
    {
        free(poller->hbuckets);
        free(poller->fds);
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

    _bb_poller_backend_destroy(poller);
    free(poller->hbuckets);
    free(poller->fds);
    free(poller);
}

static int _bb_poller_grow(bb_poller_t *poller)
{
    int new_capacity = poller->capacity * 2;
    _bb_poll_fd_t *grown = realloc(poller->fds, (size_t)new_capacity * sizeof(*grown));

    if (!grown)
    {
        return -1;
    }

    poller->fds = grown;
    poller->capacity = new_capacity;

    return 0;
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

    int idx = _bb_poller_find(poller, fd);

    if (idx < 0 && poller->count >= poller->capacity && _bb_poller_grow(poller) != 0)
    {
        return -1;
    }

    if (_bb_poller_backend_register(poller, fd, events) != 0)
    {
        return -1;
    }

    if (idx >= 0)
    {
        poller->fds[idx].events |= events;
    }
    else
    {
        if ((unsigned)poller->count >= poller->hbucket_count)
        {
            _bb_poller_hash_grow(poller);
        }

        poller->fds[poller->count].fd = fd;
        poller->fds[poller->count].events = events;
        _bb_poller_hash_insert(poller, poller->count);
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

    int idx = _bb_poller_find(poller, fd);

    if (idx < 0)
    {
        return -1;
    }

    /* Best-effort at the OS level (matches historical behavior of never
     * checking the removal syscall's return value); our own bookkeeping
     * below is always kept consistent regardless. */
    _bb_poller_backend_unregister(poller, fd, events);

    poller->fds[idx].events &= ~events;

    if (poller->fds[idx].events == 0)
    {
        int last = poller->count - 1;

        /* Take idx out of the index, then fill the hole with the last entry
         * (keeping fds[] dense) and repoint whatever linked to that entry. */
        _bb_poller_hash_relink(poller, idx, poller->fds[idx].hnext);

        if (idx != last)
        {
            _bb_poller_hash_relink(poller, last, idx);
            poller->fds[idx] = poller->fds[last];
        }

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

    return _bb_poller_backend_wait(poller, events, max_events, timeout_ms);
}
