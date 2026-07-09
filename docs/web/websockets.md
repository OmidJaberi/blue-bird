# WebSockets

Blue-Bird provides a lightweight WebSocket implementation built on top of the asynchronous connection layer.

The same `bb_websocket_t` API is used for both client and server connections.

---

# Architecture

A WebSocket connection begins as a normal HTTP request.

When the server receives a request matching a registered WebSocket route, it performs the WebSocket handshake and creates a `bb_websocket_t` instance.

```txt
Client
   |
HTTP Upgrade Request
   |
Server
   |
Router
   |
WebSocket Route
   |
bb_websocket_t
   |
Async Connection
```

Internally, the WebSocket implementation is responsible for:

- HTTP upgrade handshake
- frame parsing
- frame generation
- message reassembly
- ping/pong handling
- connection lifecycle

---

# Server WebSocket Routes

WebSocket endpoints are registered separately from normal HTTP routes.

Example:

```c
bb_error_t echo_handler(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    if (msg->type == BB_WS_MESSAGE_TEXT)
    {
        return bb_websocket_send_text(ws, msg->data);
    }

    return BB_SUCCESS();
}

bb_server_add_websocket(
    server,
    "/echo",
    echo_handler
);
```

Whenever a client connects to `/echo`, the server automatically:

1. validates the WebSocket upgrade request
2. completes the handshake
3. creates a `bb_websocket_t`
4. dispatches incoming messages to the registered handler

---

# Creating a Client

A WebSocket client is created independently of the HTTP client.

```c
bb_websocket_t *ws = bb_websocket_create();
```

To connect:

```c
bb_websocket_connect(
    ws,
    "ws://127.0.0.1:8080/echo",
    connect_callback,
    NULL // UserData, passed to callback
);
```

Connection establishment is asynchronous.

The supplied callback is invoked once the handshake either succeeds or fails.

---

# Receiving Messages

Applications receive incoming messages through a message callback.

Example:

```c
bb_error_t message_handler(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    if (msg->type == BB_WS_MESSAGE_TEXT)
    {
        printf("%s\n", (const char *) msg->data);
    }

    return BB_SUCCESS();
}

bb_websocket_set_message_callback(
    ws,
    message_handler,
    NULL // UserData
);
```

Message callbacks are invoked whenever a complete WebSocket message has been received.

---

# Sending Messages

Text messages:

```c
bb_websocket_send_text(ws, "Hello");
```

Binary messages:

```c
bb_websocket_send_binary(ws, buffer, length);
```

Messages are automatically framed before transmission.

---

# Message Types

Incoming messages are represented by `bb_ws_message_t`.

```c
typedef struct
{
    bb_ws_message_type_t type;

    const void *data;
    size_t length;
} bb_ws_message_t;
```

Supported message types include:

- `BB_WS_MESSAGE_TEXT`
- `BB_WS_MESSAGE_BINARY`
- `BB_WS_MESSAGE_CLOSE`
- `BB_WS_MESSAGE_PING`
- `BB_WS_MESSAGE_PONG`

The payload is exposed through `data` and its size through `length`.

---

# Ping and Pong

Applications may send ping frames to verify connection health.

```c
bb_websocket_send_ping(ws, payload, length);
```

Pong frames can be handled through a dedicated callback.

```c
bb_websocket_set_pong_callback(
    ws,
    pong_callback,
    NULL // UserData
);
```

---

# Closing a Connection

Connections may be closed gracefully.

```c
bb_websocket_close(ws);
```

or with an explicit status code and reason.

```c
bb_websocket_send_close(ws, 1000, "Normal Closure");
```

---

# Runtime Integration

WebSockets are fully integrated with the Blue-Bird runtime.

Each `bb_websocket_t` is associated with a runtime, either by explicitly providing one or by using the default runtime.

Example:

```c
bb_runtime_t *runtime = bb_runtime_create();

bb_websocket_t *ws = bb_websocket_create_on_runtime(runtime);

/* Or simply:
 *
 * bb_websocket_t *ws = bb_websocket_create();
 *
 * which uses the default runtime.
 */

bb_websocket_set_message_callback(
    ws,
    message_handler,
    NULL
);

bb_websocket_connect(
    ws,
    "ws://127.0.0.1:8080/echo",
    connect_callback,
    NULL
);

bb_runtime_run(runtime);
```

In a typical application, the runtime already exists before WebSocket connections are created. WebSockets remain active for as long as their associated runtime is running and are typically destroyed during application shutdown.

Server-side WebSocket connections are managed automatically by the HTTP server and use the server's runtime.

---

# Relationship with the HTTP Server

WebSocket routes are integrated directly into the HTTP server.

Incoming requests are first processed as HTTP requests.

If a request is identified as a valid WebSocket upgrade targeting a registered WebSocket route, the server upgrades the connection and transfers control to the WebSocket subsystem.

Normal HTTP routes and WebSocket routes can coexist on the same server.

---

# Design Goals

The WebSocket implementation is designed to provide:

- asynchronous operation
- lightweight message abstractions
- automatic HTTP upgrade handling
- shared runtime integration
- minimal API surface
- efficient frame processing