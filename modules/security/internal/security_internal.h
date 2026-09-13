#ifndef BB_SECURITY_INTERNAL_H
#define BB_SECURITY_INTERNAL_H

#include "blue-bird/error/error.h"
#include "blue-bird/security/session.h" /* BB_SESSION_ID_SIZE, BB_SESSION_DEFAULT_TTL */

#define BB_PASSWORD_ALGO "pbkdf2-sha256"

/* Salt/hash sizes, in bytes. A 16-byte salt and a 32-byte (SHA-256-sized)
 * derived key are the values recommended by NIST SP 800-132/OWASP for
 * PBKDF2-based password storage. These are implementation details of
 * the record format, not policy - they are not exposed through
 * bb_security_config_t. */
#define BB_PASSWORD_SALT_SIZE 16
#define BB_PASSWORD_HASH_SIZE 32

#define BB_SESSION_STORE_INITIAL_CAPACITY 32

/*
 * Defaults and validation bounds for bb_security_config_t
 * (blue-bird/security/config.h). These used to be the hardcoded values
 * throughout the module; now they're just what bb_security_config_default()
 * hands back, and what bb_security_config_validate() measures a caller's
 * config against.
 */

/* Default: OWASP's 2023 minimum recommendation for PBKDF2-HMAC-SHA256. */
#define BB_PASSWORD_PBKDF2_ITERATIONS_DEFAULT 600000
/* Floor: still meaningfully expensive; below this we consider the
 * password store to no longer have adequate brute-force resistance. */
#define BB_PASSWORD_PBKDF2_ITERATIONS_MIN 100000

/* Default: generous for legitimate use (NIST SP 800-63B asks that at
 * least 64 characters be accepted) while still bounding the size of
 * input pushed through the password pipeline per attempt. */
#define BB_PASSWORD_MAX_LENGTH_DEFAULT 256
#define BB_PASSWORD_MAX_LENGTH_MIN 1
#define BB_PASSWORD_MAX_LENGTH_CEILING 4096

/* Default: unchanged from the fixed value this module used before
 * Phase 4. */
#define BB_SESSION_LIFETIME_DEFAULT_SECONDS BB_SESSION_DEFAULT_TTL
#define BB_SESSION_LIFETIME_MIN_SECONDS 1
/* Ceiling: 30 days. Generous enough for "remember me"-style sessions
 * without making an effectively-permanent session one config typo away. */
#define BB_SESSION_LIFETIME_MAX_SECONDS (30 * 24 * 60 * 60)

/* Default: the full ID buffer (maximum entropy) - unchanged from this
 * module's fixed behavior before Phase 4. */
#define BB_SESSION_ID_LENGTH_DEFAULT (BB_SESSION_ID_SIZE - 1)
/* Floor: 32 hex chars = 128 bits, the generally-recommended minimum for
 * an unguessable session identifier. */
#define BB_SESSION_ID_LENGTH_MIN 32
/* Ceiling: the ID has to fit in the fixed bb_session_t.id buffer
 * alongside its NUL terminator. */
#define BB_SESSION_ID_LENGTH_MAX (BB_SESSION_ID_SIZE - 1)

#endif
