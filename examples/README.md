# Blue-Bird Examples

Four small apps, roughly in order of increasing scope, each building on
ideas from the last. If you're new to Blue-Bird, read them in this order:

| Example | What it adds | Modules used |
|---|---|---|
| [`hello`](hello/README.md) | Routing, request/response basics | `web` |
| [`async_sample`](async_sample/README.md) | The event loop underneath everything else (no HTTP) | `runtime` |
| [`todo`](todo/README.md) | Persistence (SQLite-backed repos), templating, middleware | `web`, `persist`, `template`, `log` |
| [`chat`](chat/README.md) | Auth/sessions, websockets, a real frontend | `web`, `persist`, `security`, `log` |

Each directory has its own `README.md` with a routes/API table and a
walkthrough of what it demonstrates; this file is just the map.

## Building

All four are discovered automatically — `examples/CMakeLists.txt` globs
every subdirectory and adds it, so a normal build produces all of them:

```bash
mkdir build && cd build
cmake ..
make
```

Each example is its own target if you only want one:

```bash
make example_hello
make example_async
make example_todo
make example_chat
```

Binaries land under `build/examples/<name>/`. `todo` and `chat` create a
SQLite file (`todo_sqlite.db`, `chat_sqlite.db`) next to their binary on
first run.

## Adding your own example

Because of the glob in `examples/CMakeLists.txt`, a new example only needs
a directory with its own `CMakeLists.txt` defining an executable target —
nothing else to register. The shape every example here follows:

```
examples/<name>/
├── CMakeLists.txt       add_executable(...) + target_link_libraries(...)
├── main.c                 create server/runtime, wire routes, run
├── include/                headers for your own app code
└── src/                    your own app code
```

`hello` is the smallest complete version of this shape; start there if
you're scaffolding something new.
