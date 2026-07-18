# Blue-Bird Error Module

## Overview

The `error` module provides a lightweight and consistent error-handling system for Blue-Bird.

It is designed around a few core principles:

- **Simple API**
- **Explicit error handling**
- **No dynamic memory allocation**
- **Stack-friendly structures**
- **Idiomatic C design**
- **Framework-wide consistency**

The module uses:
- an enum (`bb_error_code_t`) for standardized error codes,
- a small struct (`bb_error_t`) for optional contextual messages,
- and helper macros for ergonomic usage.

---

# Philosophy

Blue-Bird separates two kinds of failures:

| Type | Description | Handling |
|------|-------------|----------|
| **Developer errors** | Invalid API usage, broken assumptions, impossible states | Assertions |
| **Runtime errors** | I/O failure, invalid requests, allocation failures | `bb_error_t` return values |

This distinction keeps the framework:
- safe during development,
- robust in production,
- and pleasant to use.

---

# Header

```c
#include <blue-bird/error/error.h>
```

---

# Error Codes

The module defines a standard set of framework-wide error codes.

```c
typedef enum {
    BB_OK = 0,
    BB_ERR_ALLOC,
    BB_ERR_NULL,
    BB_ERR_BAD_REQUEST,
    BB_ERR_NOT_FOUND,
    BB_ERR_INTERNAL,
    BB_ERR_IO,
    BB_ERR_UNKNOWN
} bb_error_code_t;
```

---

## Error Code Reference

| Error Code | Description |
|------------|-------------|
| `BB_OK` | Operation succeeded |
| `BB_ERR_ALLOC` | Memory allocation failed |
| `BB_ERR_NULL` | Null pointer or invalid reference |
| `BB_ERR_BAD_REQUEST` | Invalid request or malformed input |
| `BB_ERR_NOT_FOUND` | Resource could not be found |
| `BB_ERR_INTERNAL` | Internal framework failure |
| `BB_ERR_IO` | Input/output failure |
| `BB_ERR_UNKNOWN` | Unknown or unspecified error |

---

# Error Structure

```c
typedef struct {
    bb_error_code_t code;
    const char *msg;
} bb_error_t;
```

## Fields

| Field | Description |
|------|-------------|
| `code` | Standardized error code |
| `msg` | Optional contextual message |

The `msg` field:
- may point to a string literal,
- may point to static memory,
- should remain valid for the lifetime of the error usage.

The module does not allocate or free memory internally.

---

# Helper Macros

## Success

```c
BB_SUCCESS()
```

Creates a successful error result.

Example:

```c
return BB_SUCCESS();
```

Equivalent to:

```c
return (bb_error_t){BB_OK, "OK"};
```

---

## Error

```c
BB_ERROR(code, message)
```

Creates an error value.

Example:

```c
return BB_ERROR(BB_ERR_IO, "Failed to read file");
```

---

## Failed Check

```c
BB_FAILED(err)
```

Checks whether an error represents failure.

Example:

```c
bb_error_t err = load_file();

if (BB_FAILED(err)) {
    printf("Error: %s\n", err.msg);
}
```

Equivalent to:

```c
(err.code != BB_OK)
```

---

# Error Strings

## `bb_strerror`

```c
const char *bb_strerror(bb_error_code_t code);
```

Returns a human-readable string for a framework error code.

Example:

```c
printf("%s\n", bb_strerror(BB_ERR_ALLOC));
```

Output:

```text
Memory allocation failed
```

---

# Basic Usage

## Returning Success

```c
bb_error_t save_user(User *user)
{
    // ...
    return BB_SUCCESS();
}
```

---

## Returning Errors

```c
bb_error_t load_config(const char *path)
{
    if (!path)
        return BB_ERROR(BB_ERR_NULL, "Path is NULL");

    return BB_SUCCESS();
}
```

---

# Error Propagation

Blue-Bird encourages explicit propagation of errors.

Example:

```c
bb_error_t initialize_server(Server *server)
{
    bb_error_t err = load_config("config.json");

    if (BB_FAILED(err))
        return err;

    return BB_SUCCESS();
}
```

This pattern is intentionally similar to Go-style error handling:
- errors are values,
- propagation is explicit,
- no exceptions are used.

---

# Middleware Example

```c
bb_error_t auth_middleware(Request *req, Response *res)
{
    if (!req)
        return BB_ERROR(BB_ERR_NULL, "Request is NULL");

    if (!is_authorized(req))
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Unauthorized");

    return BB_SUCCESS();
}
```

Usage:

```c
bb_error_t err = auth_middleware(req, res);

if (BB_FAILED(err)) {
    printf("Middleware failed: %s\n",
           err.msg ? err.msg : bb_strerror(err.code));
}
```

---

# Router Example

```c
bb_error_t add_route_to_list(RouteList *route_list,
                             const char *method,
                             const char *path,
                             RouteHandler handler)
{
    if (!route_list)
        return BB_ERROR(BB_ERR_NULL, "Route list is NULL");

    route_list->list[route_list->route_count].method = method;
    route_list->list[route_list->route_count].path = path;
    route_list->list[route_list->route_count].handler = handler;
    route_list->route_count++;

    return BB_SUCCESS();
}
```

Usage:

```c
bb_error_t err = add_route_to_list(&router,
                                   "GET",
                                   "/",
                                   home_handler);

if (BB_FAILED(err)) {
    fprintf(stderr, "Failed to add route: %s\n",
            err.msg ? err.msg : bb_strerror(err.code));
}
```

---

# Assertions

The error module is intended to work together with the Blue-Bird assertion system.

Assertions should be used for:
- invalid framework states,
- broken invariants,
- impossible conditions,
- developer misuse.

Error returns should be used for:
- recoverable runtime failures,
- I/O problems,
- invalid user input,
- external failures.

Example:

```c
BB_ASSERT_MSG(route_list != NULL, "Route list is NULL");
```

---

# Design Notes

## Why not exceptions?

Blue-Bird follows traditional systems-level C design:
- predictable control flow,
- explicit handling,
- no hidden stack unwinding,
- no runtime overhead from exceptions.

---

## Why use a struct instead of only enums?

Using `bb_error_t` provides:
- future extensibility,
- contextual messages,
- readable logging,
- structured propagation.

At the same time, the structure remains:
- lightweight,
- allocation-free,
- stack-friendly.

---

# Best Practices

## Recommended

```c
bb_error_t err = do_something();

if (BB_FAILED(err))
    return err;
```

---

## Recommended

```c
return BB_ERROR(BB_ERR_IO, "Failed to read file");
```

---

## Avoid

```c
fprintf(stderr, "Error!\n");
return;
```

Use structured error values instead.

---

# Summary

The Blue-Bird error system is designed to be:

- explicit,
- lightweight,
- allocation-free,
- extensible,
- idiomatic C,
- and easy to integrate across all framework modules.

It provides a consistent foundation for:
- runtime failures,
- middleware propagation,
- router logic,
- persistence modules,
- asynchronous runtime systems,
- and future framework extensions.
