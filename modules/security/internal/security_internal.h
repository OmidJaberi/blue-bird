#ifndef BB_SECURITY_INTERNAL_H
#define BB_SECURITY_INTERNAL_H

#include "blue-bird/error/error.h"

#define BB_PASSWORD_ALGO "pbkdf2-sha256"

/* Salt/hash sizes, in bytes. A 16-byte salt and a 32-byte (SHA-256-sized)
 * derived key are the values recommended by NIST SP 800-132/OWASP for
 * PBKDF2-based password storage. */
#define BB_PASSWORD_SALT_SIZE 16
#define BB_PASSWORD_HASH_SIZE 32

/* Default PBKDF2-HMAC-SHA256 iteration count. This follows OWASP's 2023
 * password storage guidance for PBKDF2-HMAC-SHA256 (>= 600,000). Phase 4
 * (security configuration) makes this overridable per-application; until
 * then it is the fixed default. */
#define BB_PASSWORD_PBKDF2_ITERATIONS 600000

#define BB_SESSION_STORE_INITIAL_CAPACITY 32

#endif
