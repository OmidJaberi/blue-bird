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

## wrk load test

```bash
tests/load/run_wrk.sh                      # uses build/tests/load/bb-loadtest-server
BB_WRK_DURATION=30s BB_WRK_CONNECTIONS=128 tests/load/run_wrk.sh
```

Starts the server, runs `wrk` against `GET /`, and cross-checks wrk's numbers
with the server's own `/metrics`. It fails on any socket error or non-2xx
response, a server crash or non-zero exit, a sanitizer/leak report in the server
log, open connections that don't drain after the run, or server counters that
disagree with wrk. It does not fail on a requests/sec threshold (shared CI
runners are too noisy). Results (`wrk.txt`, `metrics-*.txt`, `server.log`,
`summary.md`) go to `build/load-results/`. All settings are environment
variables, listed at the top of the script. Requires `wrk` and `curl`.

The server closes the connection after every response, so wrk runs with
`Connection: close` and each request is a fresh connection.

## Known limits

- A text message containing an embedded NUL byte is echoed truncated at that
  byte (the public API only has `bb_websocket_send_text(const char *)`).
- Messages of 64 KiB (65536 bytes) or more are not echoed; 65535 bytes and below
  are. This is library behaviour, not something this tool adds.
