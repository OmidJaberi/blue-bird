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
log, open connections that don't drain after the run, server file descriptors
that don't return to their pre-run count (Linux, via `/proc/<pid>/fd`; a
descriptor leak is invisible to LeakSanitizer/Valgrind when the leaked
connections are freed at shutdown), or server counters that disagree with wrk. It does not fail on a requests/sec threshold (shared CI
runners are too noisy). Results (`wrk.txt`, `metrics-*.txt`, `server.log`,
`summary.md`) go to `build/load-results/`. All settings are environment
variables, listed at the top of the script. Requires `wrk` and `curl`.

CI runs this twice (see `.github/workflows/cmake-multi-platform.yml`): a Release
build, and a Debug build with `BB_ENABLE_SANITIZERS=ON` (ASan + UBSan). The
sanitizer run also catches memory errors and, because the server shuts down
cleanly on SIGTERM, leaks (LeakSanitizer exits non-zero and the script reports
it). To reproduce it locally:

```bash
cmake -B build-asan -S . -DCMAKE_BUILD_TYPE=Debug -DBB_ENABLE_SANITIZERS=ON
cmake --build build-asan --target bb-loadtest-server
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  tests/load/run_wrk.sh build-asan/tests/load/bb-loadtest-server
```

The server closes the connection after every response, so wrk runs with
`Connection: close` and each request is a fresh connection.

## Known limits

- A text message containing an embedded NUL byte is echoed truncated at that
  byte (the public API only has `bb_websocket_send_text(const char *)`).
