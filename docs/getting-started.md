# Getting Started

This guide walks through building and running your first Blue-Bird application.

---

# Requirements

- C11 compatible compiler
- CMake 3.20+
- SQLite3 (optional)

---

# Building Blue-Bird

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

# Minimal HTTP Server

```c
#include <blue-bird/web/server.h>

bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello, Blue-Bird :)");
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

# Running Examples

Example applications are located in:

```txt
examples/
```

After building, run binaries from the build directory.

---

# Next Steps

- Read the [Architecture Guide](architecture.md)
- Learn about [Routing](web/routing.md)
- Learn about [Middleware](web/middleware.md)
- Explore the [Persistence System](persist/overview.md)