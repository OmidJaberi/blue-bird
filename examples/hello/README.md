# Blue-Bird Hello Example

The smallest possible Blue-Bird app: one file of handlers, no persistence,
no middleware. Good starting point for seeing how routing and
request/response work before moving on to `todo` or `chat`.

## Running

From the repository root:

```bash
mkdir build && cd build
cmake ..
make example_hello
./examples/hello/example_hello
```

Then:

```bash
curl http://localhost:8080/
curl http://localhost:8080/hello
curl "http://localhost:8080/hello?name=Blue"
curl -X POST http://localhost:8080/hello
```

## Layout

```
examples/hello/
├── main.c                   creates the server, registers 3 routes, runs it
├── include/app_handlers.h
└── src/app_handlers.c        the 3 handlers
```

## What it shows

| Method | Path     | Handler             | Demonstrates                          |
|--------|----------|----------------------|----------------------------------------|
| GET    | `/`      | `root_handler`        | Minimal handler: set header + body     |
| GET    | `/hello` | `hello_get_handler`   | Reading a query param (`?name=...`) with `bb_request_get_query_param` |
| POST   | `/hello` | `hello_post_handler`  | Routing the same path differently per HTTP method |

`main.c` is the whole app lifecycle in miniature:

```c
bb_server_t *server = bb_server_create(8080);
bb_server_add_route(server, "GET", "/", root_handler);
bb_server_add_route(server, "POST", "/hello", hello_post_handler);
bb_server_add_route(server, "GET", "/hello", hello_get_handler);
bb_server_start(server);
bb_runtime_run_default();
```

There's no `bb_persist`, no `bb_security`, no middleware — `bb_server_start`
followed by `bb_runtime_run_default()` is all that's required to serve
requests. Every other example builds outward from this same skeleton.
