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
bb$algorithm$salt$hash
```

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

Potential future algorithms include:

- PBKDF2
- bcrypt
- scrypt
- Argon2id

---

# Security Notes

Applications should:

- use strong passwords
- avoid logging passwords
- never store plaintext credentials
- regularly review password policies

The security module handles hashing and verification only.
