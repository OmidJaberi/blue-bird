# Persistence Overview

The persistence module provides backend-agnostic storage infrastructure.

---

# Features

- Key-value persistence
- Object-model persistence
- Repository APIs
- Schema metadata
- Multiple storage backends

---

# Persistence Layers

```txt
Repository
   ↓
Model API
   ↓
Backend
   ↓
Storage
```

---

# Supported Backends

Current and planned backends include:
- file storage (key-value only)
- JSON
- SQLite

---

# Key-Value Persistence

The key-value system provides simple storage APIs for:
- configuration
- metadata
- lightweight persistence

---

# Object Persistence

![Object Persist](../assets/object-persist.svg)

The object-model persistence system uses schemas to persist structured entities.

This enables:
- reusable repositories
- backend-independent models
- structured serialization

---

# Querying

Repositories support `WHERE`/`ORDER BY`/`LIMIT`/`OFFSET` search via
`bb_repo_find()` — pushed down to SQL on backends that support it, and
falling back to an in-memory filter on backends that don't. See
[Querying](query.md).