#ifndef BB_UTILS_HASH_H
#define BB_UTILS_HASH_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>

#define BB_SHA1_DIGEST_LENGTH 20

void bb_sha1(const void *data, size_t length, unsigned char digest[BB_SHA1_DIGEST_LENGTH]);

#define BB_SHA256_DIGEST_LENGTH 32
#define BB_SHA256_BLOCK_SIZE 64

/*
 * Streaming SHA-256 context. Callers that need to hash data
 * incrementally (or need to avoid the one-shot bb_sha256()'s internal
 * allocation in a hot loop) can use bb_sha256_init()/_update()/_final()
 * directly.
 */
typedef struct
{
    uint32_t state[8];
    uint64_t bit_length;
    unsigned char buffer[BB_SHA256_BLOCK_SIZE];
    size_t buffer_len;
} bb_sha256_ctx;

void bb_sha256_init(bb_sha256_ctx *ctx);
void bb_sha256_update(bb_sha256_ctx *ctx, const void *data, size_t length);
void bb_sha256_final(bb_sha256_ctx *ctx, unsigned char digest[BB_SHA256_DIGEST_LENGTH]);

/* One-shot convenience wrapper around the streaming context above. */
void bb_sha256(const void *data, size_t length, unsigned char digest[BB_SHA256_DIGEST_LENGTH]);

/*
 * Streaming HMAC-SHA256 (RFC 2104) context. Priming the ipad/opad state
 * (bb_hmac_sha256_init) is the expensive part of HMAC; once primed, a
 * context can be cheaply *copied* (it's a plain struct) and reused to
 * authenticate many different messages under the same key without
 * repeating that setup work. This is what makes PBKDF2 (which calls
 * HMAC under the same key up to millions of times) practical without
 * an external crypto library.
 */
typedef struct
{
    bb_sha256_ctx inner;        /* primed with ipad, then fed the message */
    bb_sha256_ctx outer_primed; /* primed with opad only - message not yet added */
} bb_hmac_sha256_ctx;

void bb_hmac_sha256_init(bb_hmac_sha256_ctx *ctx, const void *key, size_t key_length);
void bb_hmac_sha256_update(bb_hmac_sha256_ctx *ctx, const void *data, size_t length);
void bb_hmac_sha256_final(bb_hmac_sha256_ctx *ctx, unsigned char digest[BB_SHA256_DIGEST_LENGTH]);

/*
 * One-shot convenience wrapper. `key`/`key_length` may be any length -
 * keys longer than the SHA-256 block size are hashed down first, per
 * the HMAC spec.
 */
void bb_hmac_sha256(
    const void *key, size_t key_length,
    const void *data, size_t data_length,
    unsigned char digest[BB_SHA256_DIGEST_LENGTH]);


#ifdef __cplusplus
}
#endif

#endif // BB_UTILS_HASH_H
