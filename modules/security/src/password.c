/*
 * Public password hashing API.
 *
 * Stored records look like:
 *
 *   bb$pbkdf2-sha256$i=600000$<hex salt>$<hex hash>
 *          ^-------------------------------------- this part is produced
 *                                                   and parsed by
 *                                                   password_backend.c
 *   ^^ "bb" is Blue-Bird's own record-format tag, so bb_password_verify()
 *      can reject anything it doesn't understand (including the
 *      framework's old, non-cryptographic hash format) before ever
 *      handing the string to the PBKDF2 backend.
 */

#include <string.h>

#include "../internal/random.h"
#include "../internal/password_backend.h"
#include "../internal/security_internal.h"
#include "blue-bird/security/session.h" /* BB_ERR_HASH_FAILED */

#define BB_PASSWORD_RECORD_TAG "bb$pbkdf2-sha256$"
#define BB_PASSWORD_RECORD_TAG_LEN (sizeof(BB_PASSWORD_RECORD_TAG) - 1)

/* Length of the "bb" tag prepended to the backend's own record. The
 * backend's record already starts with '$', so "bb" + "$pbkdf2-..."
 * reads as "bb$pbkdf2-...". */
#define BB_PASSWORD_TAG_PREFIX_LEN 2

bb_error_t bb_password_hash(const char *password, char *out, size_t out_size)
{
    if (!password || !out)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    unsigned char salt[BB_PASSWORD_SALT_SIZE];

    bb_error_t err = _bb_random_bytes(salt, sizeof(salt));

    if (BB_FAILED(err))
        return err;

    size_t needed = BB_PASSWORD_TAG_PREFIX_LEN + _bb_password_backend_encoded_len(
        sizeof(salt),
        BB_PASSWORD_PBKDF2_ITERATIONS);

    if (out_size < needed)
        return BB_ERROR(BB_ERR_HASH_FAILED, "output buffer too small for password hash");

    out[0] = 'b';
    out[1] = 'b';

    return _bb_password_backend_hash(
        password,
        salt, sizeof(salt),
        BB_PASSWORD_PBKDF2_ITERATIONS,
        out + BB_PASSWORD_TAG_PREFIX_LEN, out_size - BB_PASSWORD_TAG_PREFIX_LEN);
}

int bb_password_verify(const char *password, const char *stored)
{
    if (!password || !stored)
        return 0;

    if (strncmp(stored, BB_PASSWORD_RECORD_TAG, BB_PASSWORD_RECORD_TAG_LEN) != 0)
        return 0; /* unsupported/unrecognized record format */

    return _bb_password_backend_verify(password, stored + BB_PASSWORD_TAG_PREFIX_LEN);
}
