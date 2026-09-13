/*
 * Public session lifecycle API: create, get, destroy, cleanup.
 *
 * This file is deliberately thin - it owns validation and the
 * expiration check on lookup, and delegates actual storage to
 * session_store.c. See session_store.c for how IDs are compared and
 * how "expired" is decided; both create/get/destroy/cleanup here and
 * the store agree on that single definition.
 */

#include <string.h>
#include <time.h>

#include "../internal/random.h"
#include "../internal/session_store.h"

bb_error_t bb_session_create(const char *user_id, time_t ttl, bb_session_t *session)
{
    if (!user_id || !session)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    memset(session, 0, sizeof(*session));

    strncpy(session->user_id, user_id, sizeof(session->user_id) - 1);

    /* A session is only as secure as its ID. If the CSPRNG fails, this
     * must fail loudly rather than hand back a session with a weak (or
     * all-zero) ID. */
    bb_error_t err = _bb_random_hex(session->id, sizeof(session->id));

    if (BB_FAILED(err))
        return err;

    session->expires_at = time(NULL) + ttl;

    return _bb_session_store_create(session);
}

bb_error_t bb_session_get(const char *id, bb_session_t *session)
{
    if (!id || !session)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    bb_error_t err = _bb_session_store_get(id, session);

    if (BB_FAILED(err))
        return err;

    if (session->expires_at <= time(NULL))
    {
        /* Found, but no longer valid. Scrub the copy we just handed
         * back rather than leaving a dead session's ID/user_id sitting
         * in the caller's (already-returned) buffer. */
        memset(session, 0, sizeof(*session));

        return BB_ERROR(BB_ERR_SESSION_EXPIRED, "expired");
    }

    return BB_SUCCESS();
}

bb_error_t bb_session_destroy(const char *id)
{
    if (!id)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    return _bb_session_store_destroy(id);
}

bb_error_t bb_session_cleanup_expired(void)
{
    return _bb_session_store_cleanup();
}
