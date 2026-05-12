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
use_pre_middleware(
    &server,
    logger_middleware
);
```

---

# Example Middleware

```c
BBError logger_middleware(request_t *req, response_t *res)
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
- metrics
- compression
- response modification

---

# Middleware Design

Middleware is designed to be:
- composable
- lightweight
- ordered
- modular