#ifndef BB_SECURITY_PASSWORD_H
#define BB_SECURITY_PASSWORD_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

#include "blue-bird/error/error.h"

#define BB_PASSWORD_HASH_MAX 256

bb_error_t bb_password_hash(const char *password, char *out, size_t out_size);

int bb_password_verify(const char *password, const char *hash);


#ifdef __cplusplus
}
#endif

#endif
