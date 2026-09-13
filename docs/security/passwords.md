# Password Management

## Overview

The password subsystem provides secure password storage and verification primitives.

Applications should never store plaintext passwords.

Instead:

```txt
password
    ↓
hash
    ↓
store hash
```

---

# Public Header

```c
#include <blue-bird/security/password.h>
```

---

# Responsibilities

The password subsystem is responsible for:

- password hashing
- password verification
- algorithm abstraction
- future hash migration support

---

# Hash Format

Password hashes are stored using a self-describing format.

Example:

```txt
bb$pbkdf2-sha256$i=600000$<hex salt>$<hex hash>
```

The algorithm is PBKDF2-HMAC-SHA256 (no external cryptography
dependency - see the module's internal SHA-256/HMAC implementation).
The iteration count is embedded directly in the record, so it always
reflects whatever cost the password was actually hashed with, even if
the configured default (see [config.md](config.md)) changes later.

This format allows future algorithm upgrades while preserving compatibility with existing hashes.

---

# Verification Flow

Password verification follows:

```txt
user password
    ↓
extract salt
    ↓
rehash input
    ↓
compare hashes
```

The original password is never stored.

---

# Design Philosophy

The hashing implementation is intentionally isolated behind internal APIs.

This allows future migration toward stronger algorithms without changing the public API.

The current implementation uses PBKDF2-HMAC-SHA256. Its cost
(iteration count) and the maximum accepted password length are
policy, not hardcoded constants - see [config.md](config.md) for
`bb_security_config_t`.

---

# Security Notes

Applications should:

- use strong passwords
- avoid logging passwords
- never store plaintext credentials
- regularly review password policies

The security module handles hashing and verification only.
