#ifndef BB_SECURITY_AUTH_H
#define BB_SECURITY_AUTH_H

#ifdef __cplusplus
extern "C" {
#endif


#include "session.h"

typedef int (*bb_auth_verify_cb)(const char *username, const char *password, char *user_id, size_t user_id_size);

bb_error_t bb_auth_login(const char *username, const char *password, bb_auth_verify_cb verify_cb, bb_session_t *session);

bb_error_t bb_auth_logout(const char *session_id);


#ifdef __cplusplus
}
#endif

#endif
