/*
 * In-memory session store.
 *
 * This file owns the actual session table and the two security-relevant
 * details of looking things up in it:
 *
 *   - ID comparison runs in constant time (see constant_time_id_equal()
 *     below) rather than short-circuiting on the first differing byte,
 *     since session IDs are bearer credentials.
 *   - "Is this session expired?" is decided in exactly one place
 *     (session_is_expired()) and used consistently by both the lookup
 *     path and the cleanup sweep, so the two can't disagree about a
 *     session sitting exactly on the expiry boundary.
 *
 * session.c (the public API) never touches store.sessions directly.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../internal/security_internal.h"
#include "../internal/session_store.h"

typedef struct
{
    bb_session_t *sessions;
    size_t count;
    size_t capacity;
} bb_memory_store_t;

static bb_memory_store_t store = {0};

static bb_error_t ensure_capacity(void)
{
    if (store.count < store.capacity)
        return BB_SUCCESS();

    size_t new_cap = store.capacity == 0 ? BB_SESSION_STORE_INITIAL_CAPACITY : store.capacity * 2;

    bb_session_t *tmp = realloc(store.sessions, new_cap * sizeof(bb_session_t));

    if (!tmp)
        return BB_ERROR(BB_ERR_ALLOC, "allocation failed");

    store.sessions = tmp;
    store.capacity = new_cap;

    return BB_SUCCESS();
}

/*
 * Compares `lookup` (an arbitrary, possibly attacker-supplied,
 * NUL-terminated string - e.g. straight out of a cookie) against
 * `stored` (one of our own session IDs, always a fully-populated
 * BB_SESSION_ID_SIZE buffer) without letting the comparison's timing
 * depend on *where* the two first differ, the way strcmp()'s does.
 *
 * This still only ever reads up to and including `lookup`'s own NUL
 * terminator - once that's seen, `lookup` is never dereferenced again,
 * so a short `lookup` string can't cause an out-of-bounds read no
 * matter how it compares against `stored`.
 */
static int constant_time_id_equal(const char *lookup, const char *stored)
{
    unsigned char diff = 0;
    int lookup_ended = 0;

    for (size_t i = 0; i < BB_SESSION_ID_SIZE; i++)
    {
        unsigned char a = 0;
        unsigned char b = (unsigned char)stored[i];

        if (!lookup_ended)
        {
            a = (unsigned char)lookup[i];

            if (a == '\0')
                lookup_ended = 1;
        }

        diff |= (unsigned char)(a ^ b);
    }

    return diff == 0;
}

static int session_is_expired(const bb_session_t *session, time_t now)
{
    return session->expires_at <= now;
}

bb_error_t _bb_session_store_create(const bb_session_t *session)
{
    bb_error_t err = ensure_capacity();

    if (BB_FAILED(err))
        return err;

    store.sessions[store.count++] = *session;

    return BB_SUCCESS();
}

bb_error_t _bb_session_store_get(const char *id, bb_session_t *session)
{
    for (size_t i = 0; i < store.count; i++)
    {
        if (constant_time_id_equal(id, store.sessions[i].id))
        {
            *session = store.sessions[i];

            return BB_SUCCESS();
        }
    }

    return BB_ERROR(BB_ERR_SESSION_NOT_FOUND, "session not found");
}

bb_error_t _bb_session_store_destroy(const char *id)
{
    for (size_t i = 0; i < store.count; i++)
    {
        if (constant_time_id_equal(id, store.sessions[i].id))
        {
            size_t last = store.count - 1;

            if (i != last)
            {
                store.sessions[i] = store.sessions[last];
            }

            /* Scrub the now-unused tail slot: it still holds a copy of
             * a live session (its own data, if the deleted session was
             * already the last slot, or a duplicate of the entry we
             * just moved into its place otherwise). Either way, a
             * session ID shouldn't linger in memory past its lifetime. */
            memset(&store.sessions[last], 0, sizeof(store.sessions[last]));

            store.count = last;

            return BB_SUCCESS();
        }
    }

    return BB_ERROR(BB_ERR_SESSION_NOT_FOUND, "session not found");
}

bb_error_t _bb_session_store_cleanup(void)
{
    time_t now = time(NULL);

    size_t j = 0;

    for (size_t i = 0; i < store.count; i++)
    {
        if (!session_is_expired(&store.sessions[i], now))
        {
            store.sessions[j++] = store.sessions[i];
        }
    }

    /* Clear the now-unused tail so expired session data (IDs
     * especially) doesn't linger in memory past its lifetime. */
    if (j < store.count)
    {
        memset(&store.sessions[j], 0, (store.count - j) * sizeof(bb_session_t));
    }

    store.count = j;

    return BB_SUCCESS();
}

size_t _bb_session_store_count(void)
{
    return store.count;
}
