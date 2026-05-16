# Models

Models represent structured application entities.

---

# Example Model

```c
typedef struct {
    bb_uuid_t id;
    char *title;
    bool completed;
} Task;
```

---

# Model Persistence

Models are persisted using:
- schemas
- repositories
- persistence backends

---

# Repositories

Repositories provide backend-independent APIs for interacting with models.

Example responsibilities:
- insert
- update
- delete
- lookup

---

# Model Lifecycle

```txt
Model
   ↓
Schema
   ↓
Repository
   ↓
Persistence Backend
```

---

# Future Directions

Planned improvements may include:
- query APIs
- indexing