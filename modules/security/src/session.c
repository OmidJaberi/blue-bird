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
#include "../internal/security_internal.h"
#include "blue-bird/security/config.h"

bb_error_t bb_session_create(const char *user_id, time_t ttl, bb_session_t *session)
{
    if (!user_id || !session)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    memset(session, 0, sizeof(*session));

    /* Reject rather than silently truncate: two distinct long user IDs
     * sharing a 63-byte prefix would otherwise collapse into one identity. */
    size_t user_id_len = strlen(user_id);

    if (user_id_len == 0 || user_id_len >= sizeof(session->user_id))
        return BB_ERROR(BB_ERR_INVALID_SECURITY_CONFIG, "invalid user id");

    /* ttl is caller-supplied and bypasses bb_security_config_t, so it gets
     * the same ceiling here (also keeps time(NULL) + ttl from overflowing).
     * A ttl <= 0 is allowed: it just yields an already-expired session,
     * which bb_session_get() rejects (the regression tests rely on this). */
    if (ttl > BB_SESSION_LIFETIME_MAX_SECONDS)
        return BB_ERROR(BB_ERR_INVALID_SECURITY_CONFIG, "session ttl exceeds maximum lifetime");

    memcpy(session->user_id, user_id, user_id_len);

    const bb_security_config_t *config = bb_security_config_get();

    /* A session is only as secure as its ID. If the CSPRNG fails, this
     * must fail loudly rather than hand back a session with a weak (or
     * all-zero) ID. */
    bb_error_t err = _bb_random_hex(session->id, config->session_id_length + 1);

    if (BB_FAILED(err))
    {
        memset(session, 0, sizeof(*session));
        return err;
    }

    session->expires_at = time(NULL) + ttl;

    err = _bb_session_store_create(session);

    /* Never hand back an ID that isn't actually in the store. */
    if (BB_FAILED(err))
        memset(session, 0, sizeof(*session));

    return err;
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
