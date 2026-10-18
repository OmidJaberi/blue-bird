#ifndef BB_UTILS_RANDOM_H
#define BB_UTILS_RANDOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/*
 * Fills `buf` with `len` bytes from the operating system's CSPRNG
 * (getrandom / getentropy / BCryptGenRandom, falling back to
 * /dev/urandom). Returns 0 on success, non-zero on failure - callers
 * must treat failure as fatal for the security-sensitive value they
 * were generating and never fall back to rand().
 */
int bb_random_bytes(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
