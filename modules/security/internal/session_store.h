#ifndef BB_SESSION_STORE_H
#define BB_SESSION_STORE_H

#include <stddef.h>

#include "blue-bird/security/session.h"

bb_error_t _bb_session_store_create(const bb_session_t *session);

bb_error_t _bb_session_store_get(const char *session_id, bb_session_t *session);

bb_error_t _bb_session_store_destroy(const char *session_id);

/* Removes every session whose expires_at is now in the past. */
bb_error_t _bb_session_store_cleanup(void);

/* Number of sessions currently held (expired or not). Exposed mainly so
 * tests can observe the effect of _bb_session_store_cleanup() directly. */
size_t _bb_session_store_count(void);

#endif
