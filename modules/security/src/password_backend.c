/*
 * PBKDF2-HMAC-SHA256 password hashing backend. No external crypto
 * dependency: built on the vendored SHA-256/HMAC-SHA256 primitives in
 * blue-bird/utils/hash.h and the PBKDF2 construction in pbkdf2.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../internal/password_backend.h"
#include "../internal/pbkdf2.h"
#include "../internal/security_internal.h"
#include "blue-bird/security/session.h" /* BB_ERR_HASH_FAILED */

#define BB_PBKDF2_RECORD_PREFIX "$pbkdf2-sha256$i="

/* Sanity bound on the salt length we'll ever decode from a record, to
 * keep decode-time stack usage fixed and small. Our own records always
 * use BB_PASSWORD_SALT_SIZE; this only guards against absurd/corrupted
 * input during verification. */
#define BB_PBKDF2_MAX_SALT_BYTES 128

static const char BB_HEX_DIGITS[] = "0123456789abcdef";

static void hex_encode(const unsigned char *data, size_t len, char *out)
{
    for (size_t i = 0; i < len; i++)
    {
        out[i * 2]     = BB_HEX_DIGITS[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = BB_HEX_DIGITS[data[i] & 0x0F];
    }
}

static int hex_nibble(char c, unsigned char *out)
{
    if (c >= '0' && c <= '9') { *out = (unsigned char)(c - '0'); return 1; }
    if (c >= 'a' && c <= 'f') { *out = (unsigned char)(c - 'a' + 10); return 1; }
    if (c >= 'A' && c <= 'F') { *out = (unsigned char)(c - 'A' + 10); return 1; }

    return 0;
}

/* Decodes exactly hex_len/2 bytes. Returns 0 on any malformed input
 * (odd length, non-hex characters) without touching `out`. */
static int hex_decode(const char *hex, size_t hex_len, unsigned char *out)
{
    if ((hex_len % 2) != 0)
        return 0;

    for (size_t i = 0; i < hex_len / 2; i++)
    {
        unsigned char hi;
        unsigned char lo;

        if (!hex_nibble(hex[i * 2], &hi) || !hex_nibble(hex[i * 2 + 1], &lo))
            return 0;

        out[i] = (unsigned char)((hi << 4) | lo);
    }

    return 1;
}

static int constant_time_equal(const unsigned char *a, const unsigned char *b, size_t len)
{
    unsigned char diff = 0;

    for (size_t i = 0; i < len; i++)
    {
        diff |= (unsigned char)(a[i] ^ b[i]);
    }

    return diff == 0;
}

static size_t iteration_digit_count(uint32_t iterations)
{
    char buf[16];

    int n = snprintf(buf, sizeof(buf), "%u", iterations);

    return (n > 0) ? (size_t)n : 0;
}

size_t _bb_password_backend_encoded_len(size_t salt_size, uint32_t iterations)
{
    return strlen(BB_PBKDF2_RECORD_PREFIX)
         + iteration_digit_count(iterations)
         + 1 /* '$' after the iteration count */
         + (salt_size * 2)
         + 1 /* '$' after the salt */
         + (BB_PASSWORD_HASH_SIZE * 2)
         + 1; /* NUL terminator */
}

bb_error_t _bb_password_backend_hash(
    const char *password,
    const unsigned char *salt, size_t salt_size,
    uint32_t iterations,
    char *output, size_t output_size)
{
    if (!password || !salt || !output)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    size_t needed = _bb_password_backend_encoded_len(salt_size, iterations);

    if (output_size < needed)
        return BB_ERROR(BB_ERR_HASH_FAILED, "output buffer too small for pbkdf2 hash");

    unsigned char hash[BB_PASSWORD_HASH_SIZE];

    _bb_pbkdf2_hmac_sha256(
        password, strlen(password),
        salt, salt_size,
        iterations,
        hash, sizeof(hash));

    int written = snprintf(output, output_size, BB_PBKDF2_RECORD_PREFIX "%u$", iterations);

    if (written < 0)
    {
        memset(hash, 0, sizeof(hash));
        return BB_ERROR(BB_ERR_HASH_FAILED, "failed to format pbkdf2 record header");
    }

    size_t pos = (size_t)written;

    hex_encode(salt, salt_size, output + pos);
    pos += salt_size * 2;

    output[pos++] = '$';

    hex_encode(hash, sizeof(hash), output + pos);
    pos += sizeof(hash) * 2;

    output[pos] = '\0';

    memset(hash, 0, sizeof(hash));

    return BB_SUCCESS();
}

int _bb_password_backend_verify(const char *password, const char *encoded)
{
    if (!password || !encoded)
        return 0;

    size_t prefix_len = strlen(BB_PBKDF2_RECORD_PREFIX);

    if (strncmp(encoded, BB_PBKDF2_RECORD_PREFIX, prefix_len) != 0)
        return 0;

    const char *p = encoded + prefix_len;

    char *end = NULL;
    unsigned long iterations_ul = strtoul(p, &end, 10);

    if (end == p || *end != '$' || iterations_ul == 0 || iterations_ul > 0xFFFFFFFFUL)
        return 0;

    uint32_t iterations = (uint32_t)iterations_ul;

    const char *salt_hex = end + 1;
    const char *sep = strchr(salt_hex, '$');

    if (!sep)
        return 0;

    size_t salt_hex_len = (size_t)(sep - salt_hex);

    if (salt_hex_len == 0 || (salt_hex_len % 2) != 0 || (salt_hex_len / 2) > BB_PBKDF2_MAX_SALT_BYTES)
        return 0;

    const char *hash_hex = sep + 1;
    size_t hash_hex_len = strlen(hash_hex);

    if (hash_hex_len != BB_PASSWORD_HASH_SIZE * 2)
        return 0;

    unsigned char salt[BB_PBKDF2_MAX_SALT_BYTES];
    size_t salt_len = salt_hex_len / 2;

    if (!hex_decode(salt_hex, salt_hex_len, salt))
        return 0;

    unsigned char stored_hash[BB_PASSWORD_HASH_SIZE];

    if (!hex_decode(hash_hex, hash_hex_len, stored_hash))
    {
        memset(salt, 0, sizeof(salt));
        return 0;
    }

    unsigned char computed_hash[BB_PASSWORD_HASH_SIZE];

    _bb_pbkdf2_hmac_sha256(
        password, strlen(password),
        salt, salt_len,
        iterations,
        computed_hash, sizeof(computed_hash));

    int match = constant_time_equal(computed_hash, stored_hash, BB_PASSWORD_HASH_SIZE);

    memset(salt, 0, sizeof(salt));
    memset(computed_hash, 0, sizeof(computed_hash));
    memset(stored_hash, 0, sizeof(stored_hash));

    return match;
}
