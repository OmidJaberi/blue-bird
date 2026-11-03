# Blue-Bird Todo Example

A classic server-rendered todo list: add, complete, and delete tasks, backed
by SQLite. This is the reference example for Blue-Bird's persistence layer
(schema-driven repos), the template engine, and middleware — `chat` and
most other non-trivial examples follow the same shape.

## Running

From the repository root:

```bash
mkdir build && cd build
cmake ..
make example_todo
./examples/todo/example_todo
```

Then open `http://localhost:8080`. By default `todo_sqlite.db` is created next
to the binary on first run.

### Choosing a backend

The same binary can store its tasks in any Blue-Bird persistence backend. Pick
one at start-up with `BB_TODO_DB`:

| `BB_TODO_DB`                     | Backend                                      |
|----------------------------------|----------------------------------------------|
| *(unset)*                        | SQLite, `todo_sqlite.db` next to the binary  |
| `sqlite:<path>`                  | SQLite at `<path>`                           |
| `json:<path>`                    | a JSON file at `<path>`                      |
| `postgresql://user:pass@host/db` | PostgreSQL                                   |

SQLite and JSON need nothing extra. PostgreSQL is opt-in: configure with
`-DBB_WITH_POSTGRES=ON` (needs libpq, see
[the PostgreSQL backend docs](../../docs/persist/postgres.md)), start a
database, and point the example at it. `compose.yaml` in this directory
provides a throwaway one:

```bash
docker compose -f examples/todo/compose.yaml up -d --wait
BB_TODO_DB=postgresql://todo:todo@127.0.0.1:5432/todo ./build/examples/todo/example_todo
docker compose -f examples/todo/compose.yaml down -v    # also deletes the data
```

A build without `-DBB_WITH_POSTGRES=ON` rejects a `postgresql://` value with an
error instead of quietly using another backend. The default build, its tests
and `ctest` never need PostgreSQL or Docker.

## Layout

```
examples/todo/
├── main.c                    wiring: db, repo, middleware, routes
├── include/
│   ├── app_repo.h              Task entity + schema
│   ├── app_handlers.h
│   └── app_middleware.h
└── src/
    ├── app_repo.c
    ├── app_handlers.c
    └── app_middleware.c
```

## Routes

| Method | Path                | Description                              |
|--------|---------------------|--------------------------------------------|
| GET    | `/`                 | Renders the task list (HTML, via the template engine) |
| POST   | `/add_task`         | Creates a task, redirects to `/`           |
| POST   | `/mark_done/:id`    | Sets a task's status to `done`, redirects to `/` |
| POST   | `/remove_task/:id`  | Deletes a task, redirects to `/`           |
| GET    | `/:id/status`       | Returns a single task as JSON              |
| GET    | `/list_tasks`       | Returns all tasks as a JSON array          |

## Persistence

One schema-driven repo (`bb_repo_t`). The backend behind it is chosen in
`main.c`; everything after that is the same for every backend:

```c
typedef struct {
    bb_uuid_t id;
    char name[64];
    char status[64];
} Task;
```

set up once in `main.c`:

```c
const bb_model_api_t *api;
bb_model_handle_t *handle;
open_backend(getenv("BB_TODO_DB"), &api, &handle);   /* SQLite, JSON or PostgreSQL */
bb_repo_init(&global_task_repo.base, api, handle, &Task_schema);
```

`open_backend()` registers the backends that were compiled in and opens the
one named by `BB_TODO_DB`; it is the only code in the example that knows which
backend is in use.

`app_repo.c` then wraps `bb_repo_insert` / `bb_repo_remove` / `bb_repo_update`
in small typed helpers (`task_insert`, `task_remove`, `task_update`), and
`get_task` / `list_tasks` show two ways to turn a `Task` into JSON: the
generic `bb_entity_to_json(&task_schema, &task)` (schema-driven, in
`serialize_task`), and building the JSON by hand field-by-field with the
`OBJ(KEY(...))` DSL (in `list_tasks` / `root`) when you want to control
exactly what's exposed.

## Templating

`root` renders HTML with the template engine instead of string-building:

```c
bb_template_t *tpl;
bb_template_parse(html_template, &tpl);

char *html;
bb_template_render(tpl, ctx, &html);
```

where `ctx` is a `bb_json_t` object built with `OBJ(KEY("tasks", task_array))`
and the template uses `{{#tasks}} ... {{/tasks}}` to loop and `{{name}}` /
`{{status}}` / `{{id}}` to interpolate — see the `html_template` string at
the top of `app_handlers.c` for the full markup.

## Middleware

`main.c` registers two middleware functions:

```c
bb_server_use_pre_middleware(server, server_header_middleware);
bb_server_use_post_middleware(server, logger_middleware);
```

- `server_header_middleware` (pre) stamps every response with a `Server`
  header before the route handler runs.
- `logger_middleware` (post) logs method, path, and final response status
  after the route handler has run, via `BB_LOG_INFO`.

## Migrations

`bb-migrate` currently drives SQLite databases only. With the JSON and
PostgreSQL backends Persist creates the table (or file) on first use and never
alters it, so a schema change there needs a manual migration.

`migrations/` holds the schema history, applied with
[`bb-migrate`](../../docs/tools/migrate.md). Persist creates the `tasks` table
on first use but never alters it, so any later schema change goes in a new
numbered file:

```bash
# from the repository root, after building with BUILD_TOOLS=ON
./build/tools/migrate/bb-migrate status --db build/examples/todo/todo_sqlite.db --dir examples/todo/migrations
./build/tools/migrate/bb-migrate up     --db build/examples/todo/todo_sqlite.db --dir examples/todo/migrations
```

`0001_create_tasks.sql` uses `CREATE TABLE IF NOT EXISTS` so it also adopts a
`todo_sqlite.db` that Persist already created.

When you change `schemas/task.schema.json`, let `bb-migrate` write the matching
migration, then review it and apply it:

```bash
./build/tools/migrate/bb-migrate generate \
    --schema examples/todo/schemas/task.schema.json \
    --dir examples/todo/migrations --name add_due
```

It generates new tables and new columns. A rename is generated when you say so
(`--rename tasks.status=state`), because it can't be told apart from a drop. A
removed or retyped column is refused (those can lose data), so write that
migration by hand. Either way,
`ctest` runs `bb-migrate verify` (test `examples.todo_migrations_match_schema`),
which fails if the manifest and the migrations disagree. You can run it
directly:

```bash
./build/tools/migrate/bb-migrate verify \
    --schema examples/todo/schemas/task.schema.json \
    --dir examples/todo/migrations
```

## Known limitation

`add_task` uses the raw POST body as the task name directly:

```c
strncpy(t.name, bb_message_get_body(msg), sizeof(t.name));
```

Blue-Bird doesn't include a form-urlencoding parser, so the body of a
standard HTML form post (`task=New+task+text`) is stored as-is, including
the `task=` prefix and `+`-for-space encoding, rather than being decoded
into just `New task text`. It's left this way deliberately to keep the
example minimal — if you need real form handling, parse
`bb_message_get_body(msg)` yourself (or switch to JSON bodies, as the
`chat` example does).
