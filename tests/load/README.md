# bb-loadtest-server

Lives in `tests/load/` (built with `BUILD_TESTS`). A small server with fixed behaviour, used by the load-test and leak-check CI jobs
(`wrk`, Autobahn, Valgrind/ASan runs under load).

| Endpoint | Behaviour |
|---|---|
| `GET /` | `200`, body `ok` |
| `GET /metrics` | Prometheus metrics (`bb_server_enable_metrics`) |
| `WS /ws` | Echoes text and binary messages |

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bb-loadtest-server
./build/tests/load/bb-loadtest-server 8090     # or: BB_LOADTEST_PORT=8090
```

The server prints `bb-loadtest-server listening on port N` when it is ready.
`SIGINT`/`SIGTERM` stop the event loop and the server is destroyed before the
process exits, so leak checkers report real leaks rather than everything still
reachable at kill time.

## Known limits

- A text message containing an embedded NUL byte is echoed truncated at that
  byte (the public API only has `bb_websocket_send_text(const char *)`).
- Messages of 64 KiB (65536 bytes) or more are not echoed; 65535 bytes and below
  are. This is library behaviour, not something this tool adds.
