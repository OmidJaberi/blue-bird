# bb-codegen

`bb-codegen` turns a small JSON manifest into generated C code. Today it
generates Persist schemas (a struct + `bb_field_t[]` + `bb_schema_t`),
removing the hand-written `offsetof()` bookkeeping that used to be the only
way to define a model.

It's built to be a **universal manifest tool**, not a persist-only script:
every manifest carries a `"kind"` field, and each kind is handled by an
independent generator registered with the tool's core. `persist.schema` is
the only kind that exists right now, but the loader/dispatcher has no
persist-specific knowledge in it — a future `web.route` or `security.policy`
kind plugs in the same way.

---

# Writing a Manifest

Manifests live next to the code that uses them (e.g.
`examples/todo/schemas/task.schema.json`), not in a central location — the
same way a `.proto` file sits next to the service that uses it.

```json
{
    "version": 1,
    "kind": "persist.schema",
    "name": "Task",
    "table": "tasks",
    "fields": [
        { "name": "id", "type": "uuid", "primary_key": true },
        { "name": "name", "type": "string", "size": 64 },
        { "name": "status", "type": "string", "size": 64 },
        { "name": "org_id", "type": "int", "references": { "schema": "Organization", "field": "id" } }
    ]
}
```

Field options: `type` (`int`, `string`, `uuid`, `blob`), `size` (required
for `string`/`blob`), `primary_key`, `required`, `unique`,
`auto_generate`, `references` (`{ "schema": ..., "field": ... }`).

---

# Generating

```bash
bb-codegen generate --out <dir> path/to/manifest.schema.json
```

Writes `<Name>_schema.generated.h` (the struct + `extern` declarations) and
`<Name>_schema.generated.c` (the `bb_field_t[]` + `bb_schema_t`
definitions) into `<dir>`. Multiple manifests can be passed at once — the
shell expands the glob, the tool doesn't do its own directory scanning.

```c
#include "Task_schema.generated.h"

bb_repo_init(&repo, api, handle, &Task_schema);
```

---

# Checking for Drift

```bash
bb-codegen check --out <dir> path/to/manifest.schema.json
```

Regenerates in memory and diffs against what's on disk. Exits non-zero and
names the stale files if a manifest was edited without regenerating —
intended as a CI step, not something you run by hand day-to-day.

---

# Generated Code Is Committed

Generated `.h`/`.c` files are committed to the repository, not produced as
part of the normal build. This was a deliberate trade-off:

- **Committed** means a fresh clone builds with nothing extra — no
  bootstrapping the codegen tool before the rest of the tree can compile,
  no editor/LSP red squiggles for headers that don't exist yet.
- The risk (a manifest edited without regenerating) is caught by
  `bb-codegen check` running as a normal test (see
  `examples/todo/CMakeLists.txt` for the `regenerate_todo_schemas` /
  `examples.todo_schema_check` pattern), the same way a linter would catch
  a formatting drift.

When you edit a manifest, regenerate manually and commit both together:

```bash
bb-codegen generate --out schemas/generated schemas/*.schema.json
git add schemas/
```

---

# Known Bootstrapping Edge Case

If the generated files for an example/app are deleted (or missing on a
fresh clone before the first `bb-codegen generate` run), CMake's configure
step fails for that target — and because it's one CMake project, that
failure currently blocks configuring the whole tree, including building
`bb-codegen` itself. Work around it by disabling examples for the
bootstrap step:

```bash
cmake -DBUILD_EXAMPLES=OFF ..
make bb-codegen
./bb-codegen generate --out path/to/schemas/generated path/to/manifest.schema.json
cmake -DBUILD_EXAMPLES=ON ..
make
```

Not yet fixed at the CMake level — tracked as a follow-up (splitting
`bb-codegen`'s own build out from the rest of the tree's configure).
