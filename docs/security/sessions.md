# Session Management

## Overview

The session subsystem provides authenticated session lifecycle management.

A session represents an authenticated user identity over time.

---

# Public Header

```c
#include <blue-bird/security/session.h>
```

---

# Session Structure

A session contains:

- unique session identifier
- user identifier
- expiration timestamp

Conceptually:

```txt
session
├── id
├── user_id
└── expires_at
```

---

# Session Lifecycle

Session management follows:

```txt
create
    ↓
lookup
    ↓
expire or destroy
```

---

# Expiration

Sessions support configurable expiration.

Example:

```txt
current time + ttl
    ↓
expires_at
```

`bb_session_create()` takes its `ttl` explicitly. `bb_auth_login()`'s
default `ttl` - and the number of hex characters generated for each
session's `id` - come from the active security configuration; see
[config.md](config.md) for `bb_security_config_t`.

Expired sessions should no longer be considered valid.

---

# Storage Backend

The MVP implementation uses:

```txt
in-memory session storage
```

The internal architecture is designed to support future backends such as:

- persist storage
- SQLite storage
- distributed session stores

without changing the public API.

---

# Cleanup

Expired sessions may be removed through cleanup operations.

```c
bb_session_cleanup_expired();
```

This prevents unbounded growth of the session store. It does not run
on a timer by itself - call it periodically from wherever your
application already drives one (see `blue-bird/security/session.h`
for a `bb_runtime_set_interval()` wiring example).

---

# Future Evolution

Potential future additions include:

- persistent session storage
- session refresh
- session metadata
- distributed session backends
- session revocation lists
