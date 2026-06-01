# Blue-Bird Web Module

The Blue-Bird web module provides lightweight HTTP server and client functionality for building networked applications in C.

It includes:
- HTTP server support
- HTTP client support
- request and response abstractions
- routing
- middleware integration
- query parameter parsing
- route parameter parsing
- HTTP message utilities

The module is designed to provide a simple and extensible API while remaining minimal and efficient.

---

# General Architecture

The web module is divided into several core components:

```txt
Client <---- HTTP ----> Server
                         |
                         +--> Router
                         |
                         +--> Middleware
                         |
                         +--> Route Handlers
```

Main architectural layers:
- HTTP client
- HTTP server
- router
- middleware system
- request/response abstraction layer

---

# HTTP Server

The HTTP server is responsible for:
- listening for incoming connections
- parsing HTTP requests
- dispatching requests to route handlers
- generating HTTP responses

Example server setup:

```c
bb_server_t *server = bb_server_create(8080);

bb_server_add_route(server, "GET", "/", root_handler);

bb_server_start(server);
```

A route consists of:
- HTTP method
- URL path
- handler callback

The server internally matches incoming requests against registered routes.

---

# Route Handlers

Handlers receive:
- a request object
- a response object

Example:

```c
bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;

    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello, Blue-Bird :)");

    return BB_SUCCESS();
}
```

Handlers can:
- read request headers
- read request body
- access route parameters
- access query parameters
- modify response status
- set response headers
- set response body

---

# Request Object

The `bb_request_t` structure represents an incoming HTTP request.

Common operations include:

```c
bb_request_set_method(&req, "GET");
bb_request_set_url(&req, "/");
bb_request_set_body(&req, "");
```

The request object supports:
- HTTP methods
- headers
- body content
- route parameters
- query parameters

Route parameters:

```c
const char *name = bb_request_get_param(req, "name");
```

Query parameters:

```c
const char *value = bb_request_get_query_param(req, "val");
```

---

# Response Object

The `bb_response_t` structure represents an outgoing HTTP response.

Example:

```c
bb_response_set_status(res, 200);

bb_response_set_header(
    res,
    "Content-Type",
    "text/plain"
);

bb_response_set_body(res, "Hello");
```

The response object supports:
- HTTP status codes
- response headers
- response body content

---

# HTTP Client

Blue-Bird also includes an HTTP client implementation for sending requests to servers.

General workflow:
1. connect client
2. send request
3. receive response
4. close connection

Example:

```c
bb_client_t client;

bb_client_connect(&client, "127.0.0.1", 8080);

bb_client_send(&client, &req);

bb_client_receive(&client, &res);

bb_client_close(&client);
```

The client works directly with:
- `bb_request_t`
- `bb_response_t`

This allows shared request/response abstractions between client and server code.

---

# HTTP Message Utilities

The module exposes lower-level HTTP message access through:

```c
bb_http_message_t
```

Example:

```c
bb_http_message_t *http_msg = bb_request_get_message(req);
```

This layer provides direct access to:
- raw headers
- body content
- body length
- parsed HTTP data

---

# Routing

The routing system maps incoming requests to handlers.

Supported features include:
- static routes
- route parameters
- multiple route parameters
- query parameter parsing
- URL decoding

Example:

```c
bb_server_add_route(&server, "GET", "/param/:name", handler);
```

---

# Middleware

The middleware system allows request preprocessing and response postprocessing.

Middleware can be used for:
- logging
- authentication
- request validation
- CORS
- metrics
- rate limiting

Middleware executes before route handlers and can:
- inspect requests
- modify requests
- modify responses
- terminate requests early

---

# Error Handling

Blue-Bird uses the `bb_error_t` abstraction for error reporting.

Example:

```c
return BB_SUCCESS();
```

This provides a unified error handling model across:
- server operations
- client operations
- request parsing
- routing
- middleware execution

---

# Module Design Goals

The web module is designed with the following goals:
- simplicity
- low overhead
- modularity
- extensibility
- readable APIs
- lightweight abstractions

It is suitable for:
- embedded systems
- lightweight APIs
- experimental servers
- educational projects
- custom networking tools

---

# Related Documentation

- [routing.md](routing.md)
- [middleware.md](middleware.md)
