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

# Errors

A middleware or a route handler can stop the pipeline by returning an error.
The client always gets a response:

- If the step that failed already set an error status (400 or above), that
  response is sent as it stands. This is how a handler answers `404` and how an
  auth middleware answers `401`.
- Otherwise the status and body become a plain `500 Internal Server Error`.
  Headers that were already set stay.

The error's message is never sent to the client. A response that ends up as a
`5xx` is logged together with that message.

The rest of the pipeline is skipped, so post-middleware does not run for a
failed request. A handler that wants post-middleware (for example, request
logging) to still run should set its status and return success instead.

```c
bb_error_t auth_middleware(bb_request_t *req, bb_response_t *res)
{
    if (!bb_request_get_header(req, "Authorization"))
    {
        bb_response_set_status(res, 401);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "missing credentials"); // 401 is sent
    }
    return BB_SUCCESS();
}
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