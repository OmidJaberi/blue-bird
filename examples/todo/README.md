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

Then open `http://localhost:8080`. `todo_sqlite.db` is created next to the
binary on first run.

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

One schema-driven repo (`bb_repo_t`) on the SQLite backend:

```c
typedef struct {
    bb_uuid_t id;
    char name[64];
    char status[64];
} Task;
```

registered once in `main.c`:

```c
bb_model_register(bb_model_sqlite_api());
const bb_model_api_t *api = bb_model_get("sqlite");
bb_model_handle_t *handle = api->open(dbfile);
bb_repo_init(&global_task_repo.base, api, handle, &task_schema);
```

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
