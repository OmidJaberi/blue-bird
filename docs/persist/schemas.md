# Schemas

Schemas describe model structure and metadata.

They provide a lightweight reflection system for persistence and serialization.

---

# Example Schema

```c
BB_Schema task_schema = {
    .name = "tasks",
    .fields = task_fields,
    .field_count = 3,
    .struct_size = sizeof(Task),
    .primary_key_index = 0
};
```

---

# Schema Responsibilities

Schemas provide:
- field metadata
- type information
- serialization metadata
- persistence metadata

---

# Fields

Schemas are composed of fields describing model attributes.

Example field metadata may include:
- field name
- type
- offset
- flags

---

# Why Schemas Exist

Schemas allow Blue-Bird to:
- serialize entities generically
- persist structured models
- support backend-independent repositories
- enable future tooling and code generation

---

# Long-Term Vision

Schemas are intended to become a foundation for:
- scaffolding
- code generation
- serialization systems
- tooling