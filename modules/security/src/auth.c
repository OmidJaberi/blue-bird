#include "blue-bird/security/auth.h"
#include "blue-bird/security/session.h"

bb_error_t bb_auth_login(const char *username, const char *password, bb_auth_verify_cb verify_cb, bb_session_t *session)
{
    char user_id[64];

    if (!username || !password || !verify_cb || !session)
    {
        return BB_ERROR(BB_ERR_NULL, "null argument");
    }

    if (!verify_cb(username, password, user_id, sizeof(user_id)))
    {
        return BB_ERROR(BB_ERR_INVALID_CREDENTIALS, "invalid credentials");
    }

    return bb_session_create(user_id, BB_SESSION_DEFAULT_TTL, session);
}

bb_error_t bb_auth_logout(const char *session_id)
{
    return bb_session_destroy(session_id);
}
