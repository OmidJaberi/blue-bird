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

The object-model persistence system uses schemas to persist structured entities.

This enables:
- reusable repositories
- backend-independent models
- structured serialization