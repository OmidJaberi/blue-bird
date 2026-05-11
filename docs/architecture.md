# Architecture

Blue-Bird is organized into modular subsystems with clear dependency direction.

---

# High-Level Structure

```txt
web
  ↓
persist
  ↓
utils
  ↓
error
```

Each layer builds on lower-level infrastructure while remaining modular and loosely coupled.

---

# Modules

## Web

The `web` module provides:
- HTTP server
- HTTP client
- routing
- middleware
- request/response abstractions

The web module is responsible for handling transport and application flow.

---

## Persist

The `persist` module provides:
- key-value persistence
- object-model persistence
- repositories
- schema metadata
- persistence backends

Persistence is backend-agnostic and designed around schema-driven models.

---

## Utils

The `utils` module provides reusable infrastructure components:
- JSON
- UUID
- time utilities
- encoding
- configuration helpers

---

## Error

The `error` module provides common error handling primitives used throughout the framework.

---

# Dependency Direction

Lower-level modules must not depend on higher-level modules.

For example:

GOOD:

```txt
persist → utils
web → persist
```

BAD:

```txt
utils → web
error → persist
```

This helps maintain modularity and architectural clarity.

---

# Request Lifecycle

```txt
Request
   ↓
Middleware
   ↓
Router
   ↓
Handler
   ↓
Response
```

Incoming HTTP requests pass through middleware before being dispatched to route handlers.

---

# Persistence Flow

```txt
Schema
   ↓
Repository
   ↓
Persistence Backend
   ↓
Storage
```

Schemas define model metadata used by repositories and persistence backends.