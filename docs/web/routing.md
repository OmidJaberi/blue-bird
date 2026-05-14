# Routing

Routing maps incoming HTTP requests to application handlers.

---

# Registering Routes

```c
bb_server_add_route(
    &server,
    "GET",
    "/",
    root_handler
);
```

Routes are registered using:
- HTTP method
- path
- handler callback

---

# Route Handlers

Handlers process incoming requests and generate responses.

Example:

```c
bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    bb_response_set_body(res, "Hello");

    return BB_SUCCESS();
}
```

---

# Route Parameters

Blue-Bird supports parameterized routes using the `:param_name` syntax.

Example route registration:

```c
bb_server_add_route(&server, "GET", "/param/:name", request_param_handler);
```

Example handler:

```c
bb_error_t request_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *name = bb_request_get_param(req, "name");

    bb_response_set_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "name: %s", name);

    bb_response_set_body(res, msg);

    return BB_SUCCESS();
}
```

Example request:

```txt
GET /param/blue-bird
```

Example response:

```txt
name: blue-bird
```

Blue-Bird also supports multiple route parameters.

Example:

```c
bb_server_add_route(&server, "GET", "/param/:param_1/:param_2", multi_request_param_handler);
```

```c
bb_error_t multi_request_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *p_1 = bb_request_get_param(req, "param_1");
    const char *p_2 = bb_request_get_param(req, "param_2");

    bb_response_set_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "%s and %s", p_1, p_2);

    bb_response_set_body(res, msg);

    return BB_SUCCESS();
}
```

Example request:

```txt
GET /param/hello/good_bye
```

Example response:

```txt
hello and good_bye
```

---

# Query Parameters

Blue-Bird supports query parameters using `bb_request_get_query_param()`.

Example route registration:

```c
bb_server_add_route(&server, "GET", "/q_param", request_query_param_handler);
```

Example handler:

```c
bb_error_t request_query_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *value = bb_request_get_query_param(req, "val");

    if (!value)
    {
        bb_response_set_status(res, 400);
        return BB_SUCCESS();
    }

    bb_response_set_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "val: %s", value);

    bb_response_set_body(res, msg);

    return BB_SUCCESS();
}
```

Example request:

```txt
GET /q_param?val=blue-bird
```

Example response:

```txt
val: blue-bird
```

Blue-Bird also supports multiple query parameters.

Example:

```c
bb_server_add_route(&server, "GET", "/q_param/multi", request_multi_query_param_handler);
```

```c
bb_error_t request_multi_query_param_handler(bb_request_t *req, bb_response_t *res)
{
    const char *value_1 = bb_request_get_query_param(req, "val_1");
    const char *value_2 = bb_request_get_query_param(req, "val_2");

    if (!value_1 || !value_2)
    {
        bb_response_set_status(res, 400);
        return BB_SUCCESS();
    }

    bb_response_set_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "%s-%s", value_1, value_2);

    bb_response_set_body(res, msg);

    return BB_SUCCESS();
}
```

Example request:

```txt
GET /q_param/multi?val_1=blue&val_2=bird
```

Example response:

```txt
blue-bird
```

Query parameters are automatically URL-decoded.

Example:

```txt
GET /q_param?val=blue%20bird%21
```

Response:

```txt
val: blue bird!
```

---

# Supported HTTP Methods

Blue-Bird supports standard HTTP methods including:
- GET
- POST
- PUT
- DELETE
- PATCH

---

# Planned Features

Future routing improvements may include:
- trie-based routing
- wildcard matching
- route grouping
