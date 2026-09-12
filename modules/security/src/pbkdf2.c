#include "../internal/pbkdf2.h"

#include <string.h>

#include <blue-bird/utils/hash.h>

/*
 * PBKDF2-HMAC-SHA256 (RFC 8018), built on the vendored bb_hmac_sha256_*
 * streaming primitives in blue-bird/utils/hash.h.
 *
 * HMAC's key-dependent setup (padding the key into ipad/opad and
 * hashing each once) is the expensive part of a single HMAC call - and
 * PBKDF2 calls HMAC under the *same* password up to millions of times.
 * So we prime that state exactly once per _bb_pbkdf2_hmac_sha256() call
 * (`base`) and cheaply struct-copy it for every iteration, rather than
 * re-deriving it from the password every time. Without this, PBKDF2 at
 * a realistic iteration count (hundreds of thousands) would take
 * seconds instead of milliseconds.
 */
void _bb_pbkdf2_hmac_sha256(
    const void *password, size_t password_len,
    const void *salt, size_t salt_len,
    uint32_t iterations,
    unsigned char *out, size_t out_len)
{
    if (out_len == 0)
        return;

    if (iterations == 0)
        iterations = 1;

    bb_hmac_sha256_ctx base;

    bb_hmac_sha256_init(&base, password, password_len);

    unsigned char index_be[4];
    unsigned char u[BB_SHA256_DIGEST_LENGTH];
    unsigned char t[BB_SHA256_DIGEST_LENGTH];

    size_t produced = 0;
    uint32_t block_index = 1;

    while (produced < out_len)
    {
        index_be[0] = (unsigned char)(block_index >> 24);
        index_be[1] = (unsigned char)(block_index >> 16);
        index_be[2] = (unsigned char)(block_index >> 8);
        index_be[3] = (unsigned char)(block_index);

        /* U1 = HMAC(password, salt || INT_32_BE(block_index)) */
        bb_hmac_sha256_ctx hmac = base;

        bb_hmac_sha256_update(&hmac, salt, salt_len);
        bb_hmac_sha256_update(&hmac, index_be, sizeof(index_be));
        bb_hmac_sha256_final(&hmac, u);

        memcpy(t, u, sizeof(t));

        /* U2..Uc, XORed together into T as we go. */
        for (uint32_t iter = 1; iter < iterations; iter++)
        {
            bb_hmac_sha256_ctx round = base;

            bb_hmac_sha256_update(&round, u, sizeof(u));
            bb_hmac_sha256_final(&round, u);

            for (size_t k = 0; k < sizeof(t); k++)
            {
                t[k] ^= u[k];
            }
        }

        size_t chunk = out_len - produced;

        if (chunk > sizeof(t))
            chunk = sizeof(t);

        memcpy(out + produced, t, chunk);

        produced += chunk;
        block_index++;
    }

    memset(&base, 0, sizeof(base));
    memset(u, 0, sizeof(u));
    memset(t, 0, sizeof(t));
}
