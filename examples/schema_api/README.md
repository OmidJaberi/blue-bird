# schema_api

**Schemas as data.** One small set of handlers serves a full REST API for
*any* entity, because everything they need -- field names, types, sizes,
offsets, flags -- is data in a `bb_schema_t`.

C has no reflection, but Blue-Bird's generated schemas give it a form of it.
`src/api.c` never mentions `Book` or `Note`; it only reads schemas. Add a
schema, register it, and the whole API exists.

## Run it

```bash
make example_schema_api
./build/examples/schema_api/example_schema_api      # listens on :8080
```

Data goes to `schema_api.db` in the working directory (SQLite) unless you pick
another backend, see [Backends](#backends).

## The API

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/` | Describes every resource and its fields |
| `GET` | `/:resource` | List, with filters, sorting and paging |
| `POST` | `/:resource` | Create (`201`) |
| `GET` | `/:resource/:id` | Read one |
| `PUT` | `/:resource/:id` | Replace one |
| `DELETE` | `/:resource/:id` | Delete (`204`) |

`:resource` is the schema's table name: here `books` and `notes`. Errors are
JSON (`{"error": "..."}`) with a proper status code.

```bash
# create -- the server generates the id
curl -X POST localhost:8080/books \
     -d '{"title":"The Dispossessed","author":"Le Guin","year":1974}'

# list: equality filter on any field, sort, page
curl 'localhost:8080/books?author=Le%20Guin&sort=year&order=desc&limit=10&offset=0'

# one entity
curl localhost:8080/books/<id>
curl -X PUT localhost:8080/books/<id> -d '{"title":"The Dispossessed","author":"Ursula K. Le Guin"}'
curl -X DELETE localhost:8080/books/<id>

# the API describes itself from the schemas
curl localhost:8080/
```

Query parameters on a list: any field name filters on equality; `sort` (a
field), `order` (`asc` or `desc`), `limit` (default 50, at most 200) and
`offset`. Rows are always ordered by primary key after the requested sort, so
paging is stable on every backend.

## How it works

Each step reads the schema instead of knowing the entity:

| Handler step | What it reads from the schema |
|---|---|
| Find the resource | `bb_schema_get(name)`, the framework's schema registry |
| Allocate the entity | `struct_size` |
| JSON in and out | `bb_json_to_entity` / `bb_entity_to_json`, driven by field types and offsets |
| Validate the body | each field's type, `size` (a string must fit), and the `REQUIRED` flag |
| Generate ids | `AUTO_GENERATE` on a `uuid` field |
| Parse `:id` | the primary key's type (`int` or string/uuid) |
| Build filters | a walk over the schema's fields: `?year=1984` becomes a typed `bb_query_where` |
| Describe the API | the same field metadata, serialised |

## Add a resource

1. Write `schemas/plant.schema.json` (see `book.schema.json`).
2. `make regenerate_schema_api_schemas`, and commit what it writes in
   `schemas/generated/`.
3. In `main.c`, include `Plant_schema.generated.h` and add
   `api_register(&Plant_schema, model, handle)`.

Rebuild, and `/plants` exists, with validation, filtering, sorting and paging.
No handler changes, and no CMake changes: every generated schema is compiled in.

## Backends

The same binary runs on any Blue-Bird persistence backend; choose it with
`BB_SCHEMA_API_DB`:

| `BB_SCHEMA_API_DB` | Backend |
|---|---|
| *(unset)* | SQLite, `schema_api.db` in the working directory |
| `sqlite:<path>` | SQLite at `<path>` |
| `json:<path>` | a JSON file at `<path>` |
| `postgresql://user:pass@host/db` | PostgreSQL (build with `-DBB_WITH_POSTGRES=ON`) |

A build without PostgreSQL support rejects a `postgresql://` value with an
error. [`../todo/compose.yaml`](../todo/compose.yaml) starts a throwaway
database: `BB_SCHEMA_API_DB=postgresql://todo:todo@127.0.0.1:5432/todo`.

## Things to know

- **Flags are applied here, not by the backends.** No backend interprets
  `REQUIRED` or `AUTO_GENERATE`; `api.c` enforces them from the schema.
- **Strings are bounded by their field size, in bytes.** Longer values are
  rejected with a `400`. `bb_json_to_entity` would truncate them silently, and
  an API should not store a shortened value without telling the client, so the
  handlers check first.
- **`PUT` replaces the entity.** Optional fields left out are reset, and the
  key always comes from the URL, never the body.
- **A missing row and a backend failure look the same** to the repo API (both
  return `-1`), so lookups report `404` for either.
- **Handlers always return success once they set a status.** The server drops
  the response of a handler that returns an error, so errors are reported as
  JSON bodies instead.
- **No authentication.** Do not expose it to an untrusted network.
- **Blob fields are not exposed** over JSON.
- **At most 8 filters** per request (`BB_QUERY_MAX_CONDITIONS`), and the names
  `sort`, `order`, `limit` and `offset` are reserved query parameters.

## Layout

```
examples/schema_api/
├── main.c                    # picks a backend, registers schemas, adds routes
├── src/api.c                 # the generic handlers
├── include/api.h
└── schemas/
    ├── book.schema.json      # manifests
    ├── note.schema.json
    └── generated/            # bb-codegen output, committed
```

`ctest` runs `bb-codegen check` (`examples.schema_api_schema_check`), which
fails if a manifest changed without regenerating.
