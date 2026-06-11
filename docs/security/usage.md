# Security Module Usage Guide

## Overview

This document demonstrates how the Blue-Bird Security module can be used inside a typical application.

The examples focus on:

- password management
- user authentication
- session creation
- session validation
- logout flows

The examples are intentionally simplified to demonstrate the API rather than complete application design.

---

# Typical Authentication Flow

A common authentication lifecycle looks like:

```txt
register user
    ↓
hash password
    ↓
store hash

user login
    ↓
verify password
    ↓
create session

authenticated requests
    ↓
lookup session

logout
    ↓
destroy session
```

---

# User Registration

Applications are responsible for storing user data.

When creating a user account:

```c
char hash[256];

bb_error_t err =
    bb_password_hash(
        password,
        hash,
        sizeof(hash)
    );
```

Store the generated hash in the application's database, repository, file, or persistence backend.

Applications should never store plaintext passwords.

---

# User Login

A login operation typically follows:

```txt
load stored password hash
    ↓
verify password
    ↓
create session
```

Example:

```c
char stored_hash[256];

/* retrieved from application storage */

if (!bb_password_verify(
        password,
        stored_hash))
{
    /* invalid password */
}
```

If verification succeeds:

```c
bb_session_t session;

bb_session_create(
    user_id,
    BB_SESSION_DEFAULT_TTL,
    &session
);
```

The application can then return the session identifier to the client.

---

# Session Validation

Authenticated requests typically provide a session identifier.

Example:

```c
bb_session_t session;

bb_error_t err =
    bb_session_get(
        session_id,
        &session
    );

if (BB_FAILED(err))
{
    /* authentication failed */
}
```

If successful:

```txt
session found
    ↓
not expired
    ↓
user authenticated
```

The application can use:

```c
session.user_id
```

to identify the authenticated user.

---

# Authentication Helper API

The authentication subsystem can simplify login flows.

Applications provide a verification callback.

Conceptually:

```txt
credentials
    ↓
application verification
    ↓
session creation
```

Example:

```c
bb_session_t session;

bb_auth_login(
    username,
    password,
    verify_callback,
    &session
);
```

The verification callback remains fully application-defined.

---

# Logout

Logout is simply session destruction.

Example:

```c
bb_auth_logout(session_id);
```

or:

```c
bb_session_destroy(session_id);
```

After destruction, the session is no longer valid.

---

# Example Application Architecture

A typical application may be structured as:

```txt
web layer
    ↓
authentication
    ↓
security module
    ↓
persistence layer
```

Example:

```txt
HTTP request
    ↓
load session
    ↓
validate user
    ↓
execute endpoint
```

The Security module intentionally does not depend on the Web module.

Applications remain responsible for integrating authentication into request handling.

---

# Ownership Model

The Security module owns:

- password hashing
- password verification
- session lifecycle management

Applications own:

- users
- databases
- repositories
- authorization
- permissions
- transport protocols

This separation keeps the module lightweight and reusable.

---

# Future Compatibility

Applications built on the current APIs should remain compatible with future additions such as:

- persistent session stores
- JWT support
- OAuth integrations
- permissions
- roles
- CSRF protection

The current API is intentionally designed to support incremental evolution.

---

# Summary

A typical usage pattern is:

```txt
register user
    ↓
hash password
    ↓
store hash

login
    ↓
verify password
    ↓
create session

authenticated request
    ↓
load session

logout
    ↓
destroy session
```

The Security module provides the authentication primitives while applications remain responsible for user storage and business logic.
