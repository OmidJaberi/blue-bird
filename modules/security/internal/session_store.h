#ifndef BB_SESSION_STORE_H
#define BB_SESSION_STORE_H

#include "blue-bird/security/session.h"

bb_error_t _bb_session_store_create(const bb_session_t *session);

bb_error_t _bb_session_store_get(const char *session_id, bb_session_t *session);

bb_error_t _bb_session_store_destroy(const char *session_id);

bb_error_t _bb_session_store_cleanup(void);

#endif
