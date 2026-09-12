#ifndef BB_SECURITY_PBKDF2_H
#define BB_SECURITY_PBKDF2_H

#include <stddef.h>
#include <stdint.h>

/*
 * PBKDF2-HMAC-SHA256 (RFC 8018), built on the vendored bb_hmac_sha256()
 * primitive in blue-bird/utils/hash.h. This has no external
 * dependencies beyond the rest of Blue-Bird.
 *
 * Derives `out_len` bytes into `out` from `password`/`salt` using
 * `iterations` rounds of HMAC-SHA256. `out_len` may be larger than the
 * SHA-256 digest size; blocks are concatenated per the RFC 8018 "F"
 * function.
 */
void _bb_pbkdf2_hmac_sha256(
    const void *password, size_t password_len,
    const void *salt, size_t salt_len,
    uint32_t iterations,
    unsigned char *out, size_t out_len);

#endif
