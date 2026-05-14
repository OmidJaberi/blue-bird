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
bb_server_t server;

init_server(&server, 8080);

add_route(&server, "GET", "/", root_handler);

start_server(&server);
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
bb_error_t root_handler(request_t *req, response_t *res)
{
    (void) req;

    set_response_header(res, "Content-Type", "text/plain");
    set_response_body(res, "Hello, Blue-Bird :)");

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

The `request_t` structure represents an incoming HTTP request.

Common operations include:

```c
set_request_method(&req, "GET");
set_request_url(&req, "/");
set_request_body(&req, "");
```

The request object supports:
- HTTP methods
- headers
- body content
- route parameters
- query parameters

Route parameters:

```c
const char *name = get_request_param(req, "name");
```

Query parameters:

```c
const char *value = get_request_query_param(req, "val");
```

---

# Response Object

The `response_t` structure represents an outgoing HTTP response.

Example:

```c
set_response_status(res, 200);

set_response_header(
    res,
    "Content-Type",
    "text/plain"
);

set_response_body(res, "Hello");
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

http_client_connect(&client, "127.0.0.1", 8080);

http_client_send(&client, &req);

http_client_receive(&client, &res);

http_client_close(&client);
```

The client works directly with:
- `request_t`
- `response_t`

This allows shared request/response abstractions between client and server code.

---

# HTTP Message Utilities

The module exposes lower-level HTTP message access through:

```c
http_message_t
```

Example:

```c
http_message_t *http_msg = &GET_REQUEST_MESSAGE(*req);
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
add_route(&server, "GET", "/param/:name", handler);
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
