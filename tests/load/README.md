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

## Valgrind under load

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DBB_ENABLE_SANITIZERS=OFF
cmake --build build --target bb-loadtest-server
BB_LOAD_OUT_DIR=build/load-results \
BB_SERVER_WRAPPER="valgrind --leak-check=full --errors-for-leak-kinds=definite,indirect \
  --error-exitcode=99 --log-file=build/load-results/valgrind.log" \
BB_START_TIMEOUT_S=60 BB_STOP_TIMEOUT_S=180 BB_WRK_TIMEOUT=30s BB_WRK_CONNECTIONS=16 \
  tests/load/run_wrk.sh
```

`BB_SERVER_WRAPPER` is a command prefix for the server (split on whitespace). Valgrind
can't be combined with ASan, so use a Debug build without sanitizers; expect it to be
10-50x slower, so keep the connection count small and raise the timeouts as above. The
script fails if Valgrind exits non-zero (`--error-exitcode`) or its log (`valgrind.log`
in the results directory) has a non-zero `ERROR SUMMARY`, on top of all the `wrk` checks.

CI runs this nightly and on demand in `.github/workflows/nightly-valgrind.yml`.
(GitHub only runs scheduled workflows from the default branch, and pauses them in
repositories with no activity for 60 days.)

> **Note: the nightly schedule is provisional.** It was chosen because Valgrind is
> slow, not because nightly is the best fit for this project, and it may change.
> A nightly failure isn't tied to the commit that caused it, and scheduled-run
> failures are easy to miss. Alternatives considered: run it on pull requests that
> touch `modules/**` or `tests/load/**` (a short run of 16 connections takes
> seconds under Valgrind, so this is probably affordable), run it manually before
> releases only, or rely on the ASan + UBSan job alone. Valgrind's main extra over
> ASan is detecting use of uninitialised memory.

## Autobahn conformance (WebSocket)

```bash
tests/load/run_autobahn.sh                 # needs Docker and python3
BB_AUTOBAHN_CASES="1.*,2.*" tests/load/run_autobahn.sh
BB_AUTOBAHN_UPDATE_EXPECTED=1 tests/load/run_autobahn.sh   # refresh the baseline
```

Runs the [Autobahn|Testsuite](https://github.com/crossbario/autobahn-testsuite)
`fuzzingclient` (from its Docker image, pinned by digest) against `WS /ws`. Results land
in `build/autobahn-results/`: the HTML report under `reports/servers/`, a flat
`results.json` (`case -> behavior/behaviorClose`) and `summary.md`. Compression cases
(12.\*, 13.\*) and the large-message cases (9.\*) are excluded by default; see the top
of the script for the environment variables.

**Gating.** The library does not pass the whole suite yet, so the run is gated against a
committed baseline, `autobahn/expected-results.json` (the `results.json` of an accepted
run). A case whose behavior or close status is worse than its baseline is a
**regression** and fails the run (CI job `autobahn`). Known failures don't fail it, and
improvements are reported. The comparison ranks statuses as: `OK`/`INFORMATIONAL` (pass)
< `NON-STRICT` < anything else (fail); an unknown status counts as a failure. A case
missing from a full run, or a new failing case the baseline doesn't know, also fails. Gating
is only applied to the default case selection; a custom `BB_AUTOBAHN_CASES` /
`BB_AUTOBAHN_EXCLUDE` is report-only unless `BB_AUTOBAHN_ENFORCE=1`.

**When you fix something**, the run reports the improved cases; rerun with
`BB_AUTOBAHN_UPDATE_EXPECTED=1` (or copy `results.json` over the baseline) and commit the
updated file so the fix is protected from now on. Moving to a newer Autobahn image
(`BB_AUTOBAHN_IMAGE`) can add or change cases, so refresh the baseline at the same time.

The script also fails if Docker/Autobahn doesn't produce a report, the server crashes,
hangs on SIGTERM or exits non-zero, or the server log contains a sanitizer or leak
report. CI runs it against the ASan + UBSan build, so it also exercises the WebSocket code
under the sanitizers, which `wrk` cannot. `test_autobahn.py` self-tests the comparison
logic and runs in CI before the suite.

**Where the library stands** (247 cases, first full run; 115 pass, 132 fail):

| Group | Failing / total |
|---|---|
| 1.\* framing | 0 / 16 |
| 2.\* ping/pong | 1 / 11 |
| 3.\* reserved bits | 6 / 7 |
| 4.\* opcodes | 10 / 10 |
| 5.\* fragmentation | 20 / 20 |
| 6.\* UTF-8 | 80 / 145 |
| 7.\* close handling | 13 / 37 |
| 10.\* misc | 1 / 1 |

These are library conformance gaps, not harness problems: fragmented messages are not
reassembled (5.\*); text is not UTF-8 validated (6.\*); reserved bits, reserved opcodes
and oversized control frames are not rejected (2.5, 3.\*, 4.\*); and invalid close
frames are not rejected with the right close code (7.\*). The server log from this run
had no sanitizer reports.

## Known limits

- A text message containing an embedded NUL byte is echoed truncated at that
  byte (the public API only has `bb_websocket_send_text(const char *)`).
