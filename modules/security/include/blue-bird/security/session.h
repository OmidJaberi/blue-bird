#ifndef BB_SECURITY_SESSION_H
#define BB_SECURITY_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif


#include <time.h>

#include "blue-bird/error/error.h"

#define BB_SESSION_ID_SIZE 64
#define BB_USER_ID_SIZE 64

#define BB_SESSION_DEFAULT_TTL 3600

typedef struct
{
    char id[BB_SESSION_ID_SIZE];
    char user_id[BB_USER_ID_SIZE];
    time_t expires_at;
} bb_session_t;

bb_error_t bb_session_create(const char *user_id, time_t ttl, bb_session_t *session);

bb_error_t bb_session_get(const char *session_id, bb_session_t *session);

bb_error_t bb_session_destroy(const char *session_id);

bb_error_t bb_session_cleanup_expired(void);


#ifdef __cplusplus
}
#endif

#endif
