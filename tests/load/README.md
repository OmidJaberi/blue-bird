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

## Autobahn conformance (WebSocket)

```bash
tests/load/run_autobahn.sh                 # needs Docker and python3
BB_AUTOBAHN_CASES="1.*,2.*" tests/load/run_autobahn.sh
```

Runs the [Autobahn|Testsuite](https://github.com/crossbario/autobahn-testsuite)
`fuzzingclient` (from its Docker image) against `WS /ws`. Results land in
`build/autobahn-results/`: the HTML report under `reports/servers/`, a flat
`results.json` (`case -> behavior/behaviorClose`) and `summary.md`.
Compression cases (12.\*, 13.\*) and the large-message cases (9.\*) are excluded
by default; see the top of the script for the environment variables.

Conformance results are **report-only** for now: a failing case is printed and
uploaded but does not fail the script. It does fail if Docker/Autobahn doesn't
produce a report, the server crashes, hangs on SIGTERM or exits non-zero, or the
server log contains a sanitizer or leak report. CI runs it against the
ASan + UBSan build (job `autobahn`), so it also exercises the WebSocket code
under the sanitizers, which `wrk` cannot.

Expect many failing cases in the first report. A quick manual probe of the echo
endpoint found these library gaps (conformance, not harness problems):

- fragmented messages are delivered frame by frame instead of being reassembled
  (categories 5.\*);
- text frames are not UTF-8 validated (6.\*);
- reserved bits, reserved opcodes, oversized control frames and unmasked client
  frames are not rejected with close code 1002 (2.\*, 3.\*, 4.\*, 5.\*);
- invalid close frames are not rejected, and every close is answered with 1000
  (7.\*).

## Known limits

- A text message containing an embedded NUL byte is echoed truncated at that
  byte (the public API only has `bb_websocket_send_text(const char *)`).
