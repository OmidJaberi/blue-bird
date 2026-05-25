# Blue-Bird

**A modular backend and web framework in C.**

## Overview

Blue-Bird is a lightweight, modular framework for building backend applications and web services in C.

The project focuses on:

- explicit architecture
- modular design
- low overhead
- composable infrastructure
- minimal dependencies

Rather than being just an HTTP server library, Blue-Bird provides a growing ecosystem of backend infrastructure components including:

- HTTP server and client
- routing and middleware
- JSON parsing and serialization
- key-value persistence
- schema-driven object-model persistence
- repositories
- Templating
- Async Runtime
- logging
- UUID and time utilities
- testing infrastructure

Blue-Bird is designed for developers who want lower-level control and transparency without sacrificing structure and maintainability.

---

# Features

## Web Framework

- HTTP server
- HTTP client
- Request/response abstractions
- Routing system
- Middleware pipeline
- Modular handler architecture

## Persistence

- Key-value persistence
- Schema-driven object persistence
- Repository APIs
- File backend
- JSON backend
- SQLite backend

## Utilities

- Full JSON parser and serializer
- UUID utilities
- Time utilities
- Encoding utilities
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
│   ├── web/        # HTTP server/client, routing, middleware
│   ├── persist/    # Key-value + object-model persistence
│   ├── template/   # Lightweight templating system
│   ├── runtime/    # Async event-driven execution system
│   ├── utils/      # JSON, UUID, time, encoding, config
│   ├── log/        # Logging infrastructure
│   └── error/      # Error handling primitives
│
├── examples/       # Example applications
├── tests/          # Unit and integration tests
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
    bb_server_t server;
    bb_server_init(&server, 8080);

    bb_server_add_route(&server, "GET", "/", root_handler);
    
    bb_server_start(&server);
    return 0;
}
```

---

# Example: JSON DSL

```c
bb_json_t *obj = BB_JSON(
    OBJ(
        KEY("name", TEXT("Bob")),
        KEY("age", INT(24)),
        KEY("active", BOOL(true))
    )
);
```

---

# Design Philosophy

Blue-Bird emphasizes:

- explicit over magical
- modular over monolithic
- composable over tightly coupled
- infrastructure-first design
- low-level transparency
- backend-focused architecture

The project aims to provide a clean and understandable backend framework ecosystem in C while remaining lightweight and extensible.

---

# Current Status

Blue-Bird is currently under active development.

The framework already includes:

- working HTTP infrastructure
- persistence systems
- JSON utilities
- repository abstractions
- examples and tests

However, APIs may still evolve as the architecture matures.

---

# Contributing

Contributions, issue reports, ideas, and discussions are welcome.

Areas of interest include:

- routing improvements
- async/event-loop support
- WebSockets
- authentication/session middleware
- additional persistence backends
- documentation and examples
- performance benchmarking
- testing infrastructure

---

# License

Blue-Bird is licensed under the MIT License.

See [LICENSE](LICENSE) for details.
