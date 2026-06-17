#ifndef BB_UTILS_HASH_H
#define BB_UTILS_HASH_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

#define BB_SHA1_DIGEST_LENGTH 20

void bb_sha1(const void *data, size_t length, unsigned char digest[BB_SHA1_DIGEST_LENGTH]);


#ifdef __cplusplus
}
#endif

#endif // BB_UTILS_HASH_H