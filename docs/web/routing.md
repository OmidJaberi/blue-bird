# Routing

Routing maps incoming HTTP requests to application handlers.

---

# Registering Routes

```c
add_route(
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
BBError root_handler(request_t *req, response_t *res)
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
add_route(&server, "GET", "/param/:name", request_param_handler);
```

Example handler:

```c
BBError request_param_handler(request_t *req, response_t *res)
{
    const char *name = get_request_param(req, "name");

    set_response_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "name: %s", name);

    set_response_body(res, msg);

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
add_route(&server, "GET", "/param/:param_1/:param_2", multi_request_param_handler);
```

```c
BBError multi_request_param_handler(request_t *req, response_t *res)
{
    const char *p_1 = get_request_param(req, "param_1");
    const char *p_2 = get_request_param(req, "param_2");

    set_response_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "%s and %s", p_1, p_2);

    set_response_body(res, msg);

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

Blue-Bird supports query parameters using `get_request_query_param()`.

Example route registration:

```c
add_route(&server, "GET", "/q_param", request_query_param_handler);
```

Example handler:

```c
BBError request_query_param_handler(request_t *req, response_t *res)
{
    const char *value = get_request_query_param(req, "val");

    if (!value)
    {
        set_response_status(res, 400);
        return BB_SUCCESS();
    }

    set_response_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "val: %s", value);

    set_response_body(res, msg);

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
add_route(&server, "GET", "/q_param/multi", request_multi_query_param_handler);
```

```c
BBError request_multi_query_param_handler(request_t *req, response_t *res)
{
    const char *value_1 = get_request_query_param(req, "val_1");
    const char *value_2 = get_request_query_param(req, "val_2");

    if (!value_1 || !value_2)
    {
        set_response_status(res, 400);
        return BB_SUCCESS();
    }

    set_response_header(res, "Content-Type", "text/plain");

    char msg[512];
    sprintf(msg, "%s-%s", value_1, value_2);

    set_response_body(res, msg);

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
