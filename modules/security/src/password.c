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
 *
 * The iteration count is read from bb_security_config_get() at hash
 * time and embedded in the record itself; bb_password_verify() reads
 * the count back out of the record it's checking, not from the current
 * config, so changing password_pbkdf2_iterations never invalidates
 * already-stored hashes.
 */

#include <string.h>

#include "../internal/random.h"
#include "../internal/password_backend.h"
#include "../internal/security_internal.h"
#include "blue-bird/security/config.h"
#include "blue-bird/security/session.h" /* BB_ERR_HASH_FAILED, BB_ERR_PASSWORD_TOO_LONG */

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

    const bb_security_config_t *config = bb_security_config_get();

    if (strlen(password) > config->password_max_length)
        return BB_ERROR(BB_ERR_PASSWORD_TOO_LONG, "password exceeds the configured maximum length");

    unsigned char salt[BB_PASSWORD_SALT_SIZE];

    bb_error_t err = _bb_random_bytes(salt, sizeof(salt));

    if (BB_FAILED(err))
        return err;

    size_t needed = BB_PASSWORD_TAG_PREFIX_LEN + _bb_password_backend_encoded_len(
        sizeof(salt),
        config->password_pbkdf2_iterations);

    if (out_size < needed)
        return BB_ERROR(BB_ERR_HASH_FAILED, "output buffer too small for password hash");

    out[0] = 'b';
    out[1] = 'b';

    return _bb_password_backend_hash(
        password,
        salt, sizeof(salt),
        config->password_pbkdf2_iterations,
        out + BB_PASSWORD_TAG_PREFIX_LEN, out_size - BB_PASSWORD_TAG_PREFIX_LEN);
}

int bb_password_verify(const char *password, const char *stored)
{
    if (!password || !stored)
        return 0;

    const bb_security_config_t *config = bb_security_config_get();

    if (strlen(password) > config->password_max_length)
        return 0; /* policy rejection - don't even attempt the backend */

    if (strncmp(stored, BB_PASSWORD_RECORD_TAG, BB_PASSWORD_RECORD_TAG_LEN) != 0)
        return 0; /* unsupported/unrecognized record format */

    return _bb_password_backend_verify(password, stored + BB_PASSWORD_TAG_PREFIX_LEN);
}
