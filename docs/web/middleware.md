# Middleware

Middleware provides a mechanism for processing requests and responses before and after route handlers execute.

---

# Middleware Pipeline

```txt
Request
   ↓
Pre Middleware
   ↓
Route Handler
   ↓
Post Middleware
   ↓
Response
```

---

# Registering Middleware

Example:

```c
bb_server_use_pre_middleware(
    &server,
    logger_middleware
);
```

---

# Example Middleware

```c
bb_error_t logger_middleware(bb_request_t *req, bb_response_t *res)
{
    printf("Incoming request\n");

    return BB_SUCCESS();
}
```

---

# Common Middleware Use Cases

Middleware can be used for:
- logging
- authentication
- request validation
- compression
- response modification

---

# Middleware and Built-in Metrics

Blue-Bird already counts requests, status classes and latency inside the server, so you don't need middleware for basic metrics. See [Metrics](metrics.md).

The optional metrics endpoint (`bb_server_enable_metrics()`) is a normal route, so pre and post middleware run around it. An authentication middleware therefore protects `/metrics` like any other route. A request rejected by middleware is still counted in the request metrics.

---

# Middleware Design

Middleware is designed to be:
- composable
- lightweight
- ordered
- modular