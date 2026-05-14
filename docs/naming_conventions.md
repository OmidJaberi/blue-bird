# Blue-Bird Naming Conventions

This document defines the official naming conventions for the Blue-Bird framework.

The primary goals of these conventions are:

- consistency
- readability
- maintainability
- C interoperability
- future language bindings support
- predictable public APIs

---

# General Philosophy

Blue-Bird follows a traditional C naming style for its public API.

The framework core is designed as:
- a stable C runtime and infrastructure layer,
- with future higher-level wrappers (such as C++) built on top.

As a result:
- the C API remains explicit and low-level,
- while wrapper layers may provide more ergonomic abstractions later.

---

# Public Symbol Namespace

All public symbols MUST be prefixed with `bb_` or `BB_`.

This prevents:
- symbol collisions,
- linker conflicts,
- namespace pollution,
- integration issues with external libraries.

---

# Type Naming

All public types use:

```txt
bb_*_t
```

This includes:
- structs
- enums
- unions
- opaque handles

## Examples

```c
bb_server_t
bb_request_t
bb_response_t

bb_json_t
bb_schema_t

bb_error_t
bb_error_code_t
```

---

# Function Naming

All public functions use:

```txt
bb_*
```

Functions use:
- snake_case
- explicit verbs
- module-oriented naming

## Examples

```c
bb_server_init()
bb_server_start()

bb_json_parse()
bb_json_stringify()

bb_model_register()
```

---

# Callback Types

Callback typedefs use:

```txt
bb_*_cb
```

The `_cb` suffix explicitly identifies callback function types.

## Examples

```c
bb_route_handler_cb
bb_middleware_cb
```

## Example Declaration

```c
typedef bb_error_t (*bb_route_handler_cb)(
    bb_request_t *req,
    bb_response_t *res
);
```

---

# Macro Naming

Macros and constants use:

```txt
BB_*
```

Macros use:
- uppercase
- underscore-separated words

## Examples

```c
BB_OK
BB_ERR_ALLOC

BB_HTTP_GET
BB_HTTP_POST

BB_SUCCESS()
BB_ERROR()
```

---

# Internal Symbols

Internal/private framework symbols use:

```txt
_bb_*
```

These symbols are NOT part of the public API and may change at any time.

## Examples

```c
_bb_router_match()
_bb_json_parse_value()
```

---

# File Naming

Source and header files use:

```txt
snake_case
```

## Examples

```txt
server.h
middleware.h
json.c
persist_sqlite.c
```

---

# Header Guards

Header guards use:

```txt
BB_<MODULE>_<NAME>_H
```

## Examples

```c
#ifndef BB_SERVER_H
#define BB_SERVER_H

#ifndef BB_PERSIST_MODEL_H
#define BB_PERSIST_MODEL_H
```

---

# C++ Compatibility

All public headers MUST include `extern "C"` guards.

## Example

```c
#ifdef __cplusplus
extern "C" {
#endif

// declarations

#ifdef __cplusplus
}
#endif
```

This ensures:
- C ABI compatibility,
- C++ interoperability,
- future wrapper support.

---

# Examples

## Good

```c
typedef struct {
    int server_fd;
} bb_server_t;

int bb_server_init(bb_server_t *server, int port);

typedef bb_error_t (*bb_route_handler_cb)(
    bb_request_t *,
    bb_response_t *
);
```

---

## Bad

```c
BBServer
BB_ModelAPI
init_server()
middlewareCb
```

These violate the established Blue-Bird naming conventions.

---

# Future Wrapper Layers

Higher-level language wrappers MAY use different naming styles.

For example:

| Language | Example |
|---|---|
| C | `bb_server_t` |
| C++ | `BB_Server` or `BB::Server` |
| Zig | `Server` |
| Rust | `BBServer` |

However, the core C API conventions defined in this document remain authoritative for the framework runtime itself.

---

# Guiding Principles

When contributing code to Blue-Bird:

- prefer explicitness over cleverness,
- keep names predictable,
- avoid abbreviations unless common,
- prioritize consistency over personal preference,
- preserve public API stability whenever possible.
