# Blue-Bird Security Module

## Overview

The Blue-Bird Security module provides foundational authentication infrastructure for backend applications.

The module is designed as:

- a lightweight security subsystem
- a password management layer
- a session management system
- a storage-agnostic authentication layer

The module intentionally focuses on core authentication concerns while remaining independent from transport and web infrastructure.

---

# Module Location

```txt
modules/security/
```

Public headers:

```txt
include/blue-bird/security/
```

---

# Design Goals

The security module is designed to provide reusable authentication infrastructure for:

- web applications
- APIs
- backend services
- future security extensions

The module prioritizes:

- explicit ownership
- modularity
- portability
- backend independence
- incremental evolution

---

# Current Features

The security module currently supports:

- password hashing
- password verification
- session creation
- session lookup
- session destruction
- configurable session expiration
- authentication helpers
- in-memory session storage
- explicit, validated security configuration (see [config.md](config.md))

---

# Dependencies

The security module may depend on:

- error
- utils
- persist

The module intentionally does not depend on:

- web
- runtime
- template
- log

This preserves clean dependency direction throughout Blue-Bird.

---

# Public Headers

```c
#include <blue-bird/security/password.h>
#include <blue-bird/security/session.h>
#include <blue-bird/security/auth.h>
#include <blue-bird/security/config.h>
```

---

# Architecture

The security module currently consists of three primary subsystems:

```txt
passwords
    ↓
authentication
    ↓
sessions
```

Applications provide user storage and credential lookup.

The security module provides password validation and session lifecycle management.

---

# Regression Coverage

Beyond the per-feature unit tests, `tests/integration/test_regression_*.c`
adversarially test the password, session, and randomness subsystems
(malformed input, corrupted records, expired/deleted/unknown sessions,
zero-length and oversized random requests), and mechanically guard
against the specific mistakes this module's security hardening fixed
(non-CSPRNG randomness, non-cryptographic password hashing) reappearing
in a future change. This suite runs, along with everything else, across
the CI matrix in `.github/workflows/cmake-multi-platform.yml`
(Linux/macOS/Windows, multiple compilers, Debug/Release, with and
without ASan/UBSan).

---

# Current Limitations

The current implementation is considered MVP infrastructure.

Current limitations include:

- in-memory session storage only
- no JWT support
- no OAuth support
- no OpenID Connect support
- no permissions system
- no CSRF protection
- no rate limiting

These may be introduced incrementally in future phases.

---

# Future Evolution

Potential future additions include:

- JWT support
- OAuth integrations
- OpenID Connect
- permissions
- roles
- CSRF protection
- rate limiting
- persistent session backends

The public API is intentionally designed to support these additions without major breaking changes.

---

# Summary

The Blue-Bird Security module provides:

- password management
- session management
- authentication helpers
- backend-independent security primitives

while remaining:

- lightweight
- modular
- explicit
- extensible


---

# Related Documentation

- [auth.md](auth.md)
- [passwords.md](passwords.md)
- [sessions.md](sessions.md)
- [config.md](config.md)
- [usage.md](usage.md)
