# TLS, HTTPS & WSS

Blue-Bird supports TLS as an optional build-time feature, giving you HTTPS and WSS (WebSocket-over-TLS) servers using the exact same HTTP parser, router, middleware, handlers, and WebSocket code as plain HTTP/WS.

TLS is implemented as a **transport abstraction** underneath the connection layer, so nothing above it — routes, middleware, handlers, WebSocket framing — needs to know or care whether a given connection is plaintext TCP or TLS/TCP.

---

# Enabling TLS

TLS is off by default and requires OpenSSL. Enable it at configure time:

```bash
cmake .. -DBB_WITH_TLS=ON
make
```

| Build                | Behavior                                                              |
|----------------------|------------------------------------------------------------------------|
| `BB_WITH_TLS=OFF` (default) | Project builds with no OpenSSL dependency. Plain HTTP/WS work as usual. `bb_server_create_tls*()` compiles fine but always fails with `BB_ERR_TLS_UNSUPPORTED`. |
| `BB_WITH_TLS=ON`     | OpenSSL is located via `find_package(OpenSSL REQUIRED)`. HTTPS/WSS become available. |

Because the API surface is identical either way, application code that calls `bb_server_create_tls_on_runtime()` never needs `#ifdef`s — it just needs to check the returned error if TLS support wasn't compiled in.

---

# Starting an HTTPS Server

```c
#include <blue-bird/web/server.h>

bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello over TLS");
    return BB_SUCCESS();
}

int main(void)
{
    bb_tls_config_t tls_config = {
        .certificate_file = "cert.pem", // certificate, or full chain (leaf first)
        .private_key_file = "key.pem",
    };

    bb_error_t err;
    bb_server_t *server = bb_server_create_tls(8443, &tls_config, &err);

    if (!server)
    {
        fprintf(stderr, "Failed to start HTTPS server: %s\n", err.message);
        return 1;
    }

    bb_server_add_route(server, "GET", "/", root_handler);

    bb_server_start(server);
    bb_runtime_run_default();
    return 0;
}
```

`bb_server_add_route`, `bb_server_add_websocket`, middleware registration, and every route handler are used exactly as they are for a plain-TCP server — see [Routing](routing.md), [Middleware](middleware.md), and [WebSockets](websockets.md).

A runtime-bound variant is also available, mirroring `bb_server_create_on_runtime()`:

```c
bb_server_t *bb_server_create_tls_on_runtime(
    bb_runtime_t *runtime,
    int port,
    const bb_tls_config_t *tls_config,
    bb_error_t *out_err
);
```

---

# Certificate & Key Validation

The certificate and private key are loaded and validated **immediately**, during server creation — never deferred until the first client connects.

`bb_server_create_tls*()` returns `NULL` and fills in `out_err` if:

- the certificate file is missing or unreadable
- the private key file is missing or unreadable
- the certificate is malformed
- the private key is malformed
- the certificate and private key don't match
- the TLS context can't be initialized (e.g. TLS support wasn't compiled in)

```c
bb_error_t err;
bb_server_t *server = bb_server_create_tls(8443, &tls_config, &err);

if (!server)
{
    // err.code is one of:
    //   BB_ERR_TLS_CONFIG      - certificate/key/context problem
    //   BB_ERR_TLS_UNSUPPORTED - built without BB_WITH_TLS
    fprintf(stderr, "%s\n", err.message);
}
```

`certificate_file` may be a single leaf certificate or a full chain (PEM, leaf certificate first, intermediates following).

---

# WSS (WebSocket over TLS)

WebSocket routes registered with `bb_server_add_websocket()` automatically work over TLS on an HTTPS server — there is no separate "WSS route" concept. A client that completes a WebSocket upgrade handshake over an already-established TLS connection gets a normal `bb_websocket_t`, and message framing, ping/pong, and close handling all behave identically to plain `ws://`.

```c
bb_error_t echo_handler(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    if (bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT)
    {
        return bb_websocket_send_text(ws, bb_ws_message_get_data(msg));
    }
    return BB_SUCCESS();
}

bb_server_t *server = bb_server_create_tls(8443, &tls_config, &err);
bb_server_add_websocket(server, "/echo", echo_handler);
```

A browser or client would connect to this endpoint with `wss://host:8443/echo`.

> **Note:** outbound client-side TLS (an app calling `bb_websocket_connect()` with a `wss://` URL, or an HTTPS `bb_client_t`) is not implemented yet — only the server/accept side. `bb_client_t` and `bb_websocket_connect()` remain plain-TCP (`http://`/`ws://`) for now.

---

# Architecture

TLS is implemented as a small transport abstraction that sits underneath `bb_connection_t`, the same layer both HTTP and WebSocket code already read and write through:

```txt
                HTTP / WebSocket
                       │
              bb_connection_t
             (read / write buffers)
                       │
               bb_transport_t
                  /        \
                 /          \
          Plain TCP     TLS/TCP
         (recv/send)   (SSL_read/write)
```

HTTP parsing, routing, middleware, and WebSocket framing only ever call `bb_connection_read()`/`bb_connection_write()`. Whether those calls end up doing `recv()`/`send()` or `SSL_read()`/`SSL_write()` is decided entirely by which transport is attached to the connection — none of that code contains OpenSSL-specific logic, or even knows OpenSSL exists.

## Handshake

A newly accepted TLS connection starts in a dedicated handshake state before it's treated as an open HTTP/WebSocket connection:

```txt
TCP accepted
      │
      ▼
TLS handshake   (BB_CONNECTION_HANDSHAKE)
      │
      ▼
HTTP / WebSocket connection   (BB_CONNECTION_READING)
```

No bytes reach the HTTP parser until the handshake completes. If it fails, the connection is closed cleanly.

## Non-Blocking I/O and Direction Flips

Everything runs on Blue-Bird's existing async runtime and event loop — TLS does not introduce a second event loop or ever block the runtime.

OpenSSL's handshake and I/O calls can require *either* read or write readiness at any point (`SSL_ERROR_WANT_READ` / `SSL_ERROR_WANT_WRITE`), regardless of which operation triggered them. For example, a read can need to wait on write-readiness mid-handshake, and a write can need to wait on read-readiness during renegotiation.

The transport layer surfaces this as a small status enum:

```c
typedef enum {
    BB_TRANSPORT_OK,
    BB_TRANSPORT_WANT_READ,
    BB_TRANSPORT_WANT_WRITE,
    BB_TRANSPORT_CLOSED,
    BB_TRANSPORT_ERROR
} bb_transport_status_t;
```

The async connection layer watches for a direction mismatch and, instead of busy-looping, flips which poller event its read/write task is registered for — waiting on `WRITE` for a read that needs it, or `READ` for a write that needs it — and resumes the original operation once that readiness fires.

## Partial Writes & Backpressure

Blue-Bird's existing output-buffering/backpressure mechanism remains authoritative. A partial `SSL_write()` (e.g. 3 KB written out of 10 KB requested) leaves the remaining bytes queued exactly as a partial plain-TCP `send()` would — no bytes are discarded, and the write resumes from where it left off once the connection is writable again.

## Shutdown & Cleanup

Connection teardown always releases the transport (`SSL *`), the socket, and connection buffers, in that order, regardless of which transport was in use. TLS shutdown (`SSL_shutdown()`) is attempted best-effort and non-blocking — Blue-Bird does not loop waiting for the peer's `close_notify` before releasing the connection.

---

# Security Notes

- Certificate verification is never disabled by Blue-Bird itself, and there is no configuration option to weaken it.
- TLS 1.0/1.1 are disabled; the minimum negotiated version is TLS 1.2, using OpenSSL's own secure cipher/version defaults above that floor.
- OpenSSL error details are logged locally through Blue-Bird's logging system and are never sent to remote clients.
- HTTPS never silently downgrades to HTTP, and WSS never silently downgrades to WS — a TLS handshake failure closes the connection instead.

---

# Out of Scope

The following are intentionally not part of this feature set:

- HTTP/2, HTTP/3, QUIC
- Certificate authority management or automatic provisioning (ACME/Let's Encrypt)
- Client certificate authentication (mTLS)
- Custom cipher negotiation or other application-level TLS tuning beyond secure defaults
- Outbound/client-side TLS (`bb_client_t` over HTTPS, `bb_websocket_connect()` over `wss://`)

---

# Related Documentation

- [Web Overview](overview.md)
- [Routing](routing.md)
- [Middleware](middleware.md)
- [WebSockets](websockets.md)
