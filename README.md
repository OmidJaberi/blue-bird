# Blue-Bird

[![CI](https://github.com/OmidJaberi/blue-bird/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/OmidJaberi/blue-bird/actions/workflows/cmake-multi-platform.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![C11](https://img.shields.io/badge/C-C11-blue)
![CMake](https://img.shields.io/badge/CMake-3.20+-brightgreen)

**A modular backend and web framework in C.**

## Overview

Blue-Bird is a modular framework for building backend applications and web services in C.

![Blue-Bird Architecture](docs/assets/blue-bird_architecture.svg)

The project focuses on:

- explicit architecture
- modular design
- low overhead
- composable infrastructure
- minimal dependencies

---

# Example: Minimal HTTP Server

```c
#include <blue-bird/web/server.h>

bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello, Blue-Bird :)");
    return BB_SUCCESS();
}

int main(void)
{
    bb_server_t *server = bb_server_create(8080);

    bb_server_add_route(server, "GET", "/", root_handler);
    
    bb_server_start(server);
    bb_runtime_run_default();
    return 0;
}
```

---

# Example: JSON DSL

```c
bb_json_t *obj = BB_JSON(
    OBJ(
        KEY("name", TEXTV("Bob")),
        KEY("age", INTV(24)),
        KEY("active", BOOLV(true))
    )
);
```

---

# Examples

For examples, checkout [here](examples/README.md). Currently the following examples are available:
| Example | What it adds | Modules used |
|---|---|---|
| [`hello`](examples/hello/README.md) | Routing, request/response basics | `web` |
| [`async_sample`](examples/async_sample/README.md) | The event loop underneath everything else (no HTTP) | `runtime` |
| [`todo`](examples/todo/README.md) | Persistence (SQLite-backed repos), templating, middleware | `web`, `persist`, `template`, `log` |
| [`chat`](examples/chat/README.md) | Auth/sessions, websockets, a real frontend | `web`, `persist`, `security`, `log` |

---

# Features

Rather than being just an HTTP server library, Blue-Bird provides a growing ecosystem of backend infrastructure components including:

## Async Runtime

![Runtime](docs/assets/runtime_architecture.svg)

- cooperative task scheduling
- event loop execution
- file descriptor readiness watching
- timer scheduling
- one-shot timeouts and periodic intervals
- one-shot and persistent event watchers
- nonblocking transport integration

## Web Framework

![Web](docs/assets/web_architecture.svg)

- Server supporting HTTP + Websockets
- HTTP client
- Websocket client
- Request/response abstractions
- Routing system
- Middleware pipeline
- Modular handler architecture
- Async Request handling

## Persistence

![Object Persist](docs/assets/object-persist.svg)

- Key-value persistence
- Schema-driven object persistence
- Repository APIs
- File backend
- JSON backend
- SQLite backend

## Templating

- variable interpolation
- nested lookup
- sections (loops)
- conditionals
- comments
- escaped delimiters

## Security

- Password hashing and verification
- Session management
- Authentication helpers
- Configurable session expiration
- Storage-agnostic authentication APIs
- Extensible security architecture

## Utilities

- Full JSON parser and serializer
- UUID utilities
- Time utilities
- Encoding and Hash utilities
- Static Assets utilities
- Platform specific abstractions
- Configuration helpers

## Infrastructure

- Logging system
- Error handling APIs
- Modular architecture
- CMake-based build system
- Unit and integration tests
- Example applications

---

# Repository Structure

```txt
blue-bird/
├── modules/
│   ├── web/        # HTTP server/client, routing, middleware, websockets
│   ├── persist/    # Key-value + object-model persistence
│   ├── template/   # Lightweight templating system
│   ├── runtime/    # Async event-driven execution system
│   ├── security/   # Authentication, passwords, sessions
│   ├── utils/      # JSON, UUID, time, encoding, config
│   ├── log/        # Logging infrastructure
│   └── error/      # Error handling primitives
│
├── examples/       # Example applications
├── tests/          # Unit and integration testing infrastructure
├── CMakeLists.txt
└── README.md
```

---

# Build

## Requirements

- C11 compatible compiler
- CMake 3.20+
- SQLite3 (optional, for SQLite persistence backend)

## Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

This builds:

- framework modules
- examples
- tests

---

# Running Tests

```bash
ctest
```

Or run test binaries directly from the generated `build/tests/` directory.

---

# Design Philosophy

Blue-Bird is designed for developers who want lower-level control and transparency without sacrificing structure and maintainability.

Blue-Bird emphasizes:

- explicit over magical
- modular over monolithic
- composable over tightly coupled
- infrastructure-first design
- low-level transparency
- backend-focused architecture

The project aims to provide a clean and understandable backend framework ecosystem in C while remaining lightweight and extensible.

---

## Why C

C forces every abstraction to be explicit — there's no framework magic hiding
allocation, control flow, or lifetime behind the scenes. Blue-Bird is built in
C because that transparency is the point: every module's behavior should be
traceable and predictable, and every dependency should be a deliberate choice,
not a transitive one. Practically, it also means no runtime to bundle, no GC
pauses, and portability to anywhere a C compiler runs — embedded targets,
older systems, constrained environments included.

---

# Current Status

Blue-Bird is currently under active development.

The framework already includes:

- Async Runtime
- working HTTP infrastructure
- working Websockets infrastructure
- authentication and session management
- persistence systems
- templating
- JSON utilities
- repository abstractions
- examples and tests

However, APIs may still evolve as the architecture matures.

---

# Contributing

Contributions, issue reports, ideas, and discussions are welcome.

---

# License

Blue-Bird is licensed under the MIT License.

See [LICENSE](LICENSE) for details.
