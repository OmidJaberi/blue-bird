# Schemas

Schemas describe model structure and metadata.

They provide a lightweight reflection system for persistence and serialization.

---

# Example Schema

```c
bb_schema_t task_schema = {
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

# Generating Schemas

Rather than hand-writing a schema's `bb_field_t[]` and matching
`offsetof()` calls, schemas can be generated from a JSON manifest with
[`bb-codegen`](../tools/codegen.md):

```json
{
    "version": 1,
    "kind": "persist.schema",
    "name": "Task",
    "table": "tasks",
    "fields": [
        { "name": "id", "type": "uuid", "primary_key": true },
        { "name": "name", "type": "string", "size": 64 }
    ]
}
```

```bash
bb-codegen generate --out schemas/generated schemas/task.schema.json
```

The `examples/todo` app uses this instead of a hand-written schema — see
`examples/todo/schemas/`.

---

# Long-Term Vision

Schemas are intended to become a foundation for:
- scaffolding
- serialization systems
- further tooling (migrations, dialect-specific validation)