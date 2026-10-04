#!/usr/bin/env bash
#
# Load-test bb-loadtest-server with wrk and cross-check wrk's numbers against the
# server's own /metrics. Fails on any socket error, non-2xx response, server
# crash or unclean exit, sanitizer/leak report, or metrics that disagree with wrk.
# It does NOT fail on a requests/sec threshold: shared CI runners are too noisy.
#
# Usage: tests/load/run_wrk.sh [path-to-bb-loadtest-server]
#
# Environment (all optional):
#   BB_LOADTEST_BIN      server binary           (default build/tests/load/bb-loadtest-server)
#   BB_LOADTEST_PORT     port                    (default 8090)
#   BB_WRK_DURATION      wrk -d                  (default 10s)
#   BB_WRK_THREADS       wrk -t                  (default 2)
#   BB_WRK_CONNECTIONS   wrk -c                  (default 64)
#   BB_WRK_PATH          request path            (default /)
#   BB_LOAD_OUT_DIR      results directory       (default build/load-results)
#   BB_FD_SLACK          extra open fds tolerated after the run (default 2)
#
# The server closes the connection after every response, so wrk is run with
# "Connection: close": each request is a fresh connection, which also exercises
# the accept/close path and the connection-list bookkeeping.

set -euo pipefail

SERVER_BIN="${1:-${BB_LOADTEST_BIN:-build/tests/load/bb-loadtest-server}}"
PORT="${BB_LOADTEST_PORT:-8090}"
DURATION="${BB_WRK_DURATION:-10s}"
THREADS="${BB_WRK_THREADS:-2}"
CONNECTIONS="${BB_WRK_CONNECTIONS:-64}"
REQ_PATH="${BB_WRK_PATH:-/}"
OUT_DIR="${BB_LOAD_OUT_DIR:-build/load-results}"

BASE_URL="http://127.0.0.1:${PORT}"

# shellcheck source=tests/load/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

require_tools wrk curl awk
require_server_bin

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR"/wrk.txt "$OUT_DIR"/metrics-*.txt "$OUT_DIR"/server.log "$OUT_DIR"/summary.md

# ---------------------------------------------------------------- start server
start_server

# ------------------------------------------------------------------- run wrk
# Baseline fd count with no client connected. A descriptor leak is invisible to
# LeakSanitizer/Valgrind when the leaked connections are freed at shutdown, so
# compare the server's open fds before and after the run instead.
sleep 0.3
fds_before=$(fd_count "$SERVER_PID" || true)

scrape "$OUT_DIR/metrics-before.txt"

echo "Running wrk: -t${THREADS} -c${CONNECTIONS} -d${DURATION} ${BASE_URL}${REQ_PATH}"
wrk_rc=0
wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" --latency --timeout 5s \
    -H "Connection: close" "${BASE_URL}${REQ_PATH}" 2>&1 | tee "$OUT_DIR/wrk.txt" || wrk_rc=${PIPESTATUS[0]}
[[ "$wrk_rc" -eq 0 ]] || fail "wrk exited with status $wrk_rc"

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    fail "server process died during the load run"
fi

# ------------------------------------------------- parse wrk output (wrk.txt)
wrk_requests=$(awk '/requests in/ { print $1; exit }' "$OUT_DIR/wrk.txt")
wrk_rps=$(awk '/^Requests\/sec:/ { print $2; exit }' "$OUT_DIR/wrk.txt")
wrk_p50=$(awk '$1 == "50%" { print $2; exit }' "$OUT_DIR/wrk.txt")
wrk_p99=$(awk '$1 == "99%" { print $2; exit }' "$OUT_DIR/wrk.txt")
wrk_socket_errors=$(awk '/Socket errors:/ { gsub(",", ""); print $4 + $6 + $8 + $10; exit }' "$OUT_DIR/wrk.txt")
wrk_non2xx=$(awk '/Non-2xx or 3xx responses:/ { print $NF; exit }' "$OUT_DIR/wrk.txt")
wrk_requests=${wrk_requests:-0}; wrk_rps=${wrk_rps:-0}; wrk_p50=${wrk_p50:-n/a}; wrk_p99=${wrk_p99:-n/a}
wrk_socket_errors=${wrk_socket_errors:-0}; wrk_non2xx=${wrk_non2xx:-0}

[[ "$wrk_requests" -gt 0 ]] || fail "wrk completed no requests"
[[ "$wrk_socket_errors" -eq 0 ]] || fail "wrk reported $wrk_socket_errors socket errors"
[[ "$wrk_non2xx" -eq 0 ]] || fail "wrk saw $wrk_non2xx non-2xx/3xx responses"

# ------------------------------------------- cross-check against server metrics
# After the run every connection should drain; the only open one is this scrape.
active=""
for _ in $(seq 1 50); do
    scrape "$OUT_DIR/metrics-after.txt"
    active=$(metric "$OUT_DIR/metrics-after.txt" bluebird_http_connections_active)
    [[ "${active:-999999}" -le 1 ]] && break
    sleep 0.1
done
[[ "${active:-999999}" -le 1 ]] || fail "bluebird_http_connections_active is ${active:-unknown} after the run (expected <= 1): connections leaked from the connection list"

# Back to the baseline (plus a little slack) once every connection has drained.
FD_SLACK="${BB_FD_SLACK:-2}"
fds_after=""
if [[ -n "$fds_before" ]]; then
    for _ in $(seq 1 50); do
        fds_after=$(fd_count "$SERVER_PID")
        [[ "${fds_after:-999999}" -le $(( fds_before + FD_SLACK )) ]] && break
        sleep 0.1
    done
    [[ "${fds_after:-999999}" -le $(( fds_before + FD_SLACK )) ]] || fail "server has $fds_after open fds after the run, up from $fds_before before it: descriptors leaked"
else
    echo "note: /proc/<pid>/fd not available on this platform; skipping the fd-leak check"
fi

delta() { echo $(( $(metric "$OUT_DIR/metrics-after.txt" "$1") - $(metric "$OUT_DIR/metrics-before.txt" "$1") )); }

d2xx=$(delta 'bluebird_http_requests_total{status_class="2xx"}')
d_other=0
for cls in 1xx 3xx 4xx 5xx other; do
    d_other=$(( d_other + $(delta "bluebird_http_requests_total{status_class=\"${cls}\"}") ))
done
d_latency=$(delta bluebird_http_request_duration_seconds_count)

# Server counts a response when it is queued, wrk when it is parsed, so the server
# is at most one in-flight request per connection ahead (+1 for the "before" scrape).
max_ahead=$(( CONNECTIONS + 2 ))
if [[ "$REQ_PATH" == "/" ]]; then
    [[ "$d2xx" -ge "$wrk_requests" ]] || fail "server counted $d2xx 2xx responses but wrk completed $wrk_requests"
    [[ "$d2xx" -le $(( wrk_requests + max_ahead )) ]] || fail "server counted $d2xx 2xx responses, more than wrk's $wrk_requests plus $max_ahead in flight"
    [[ "$d_other" -le 1 ]] || fail "server counted $d_other non-2xx responses during the run"
fi
[[ "$d_latency" -eq $(( d2xx + d_other )) ]] || fail "latency histogram count ($d_latency) disagrees with request counters ($(( d2xx + d_other )))"

# ------------------------------------------------------------------ stop server
stop_server

# --------------------------------------------------------------------- summary
{
    echo "### wrk load test"
    echo
    echo "| | |"
    echo "|---|---|"
    echo "| Config | \`-t${THREADS} -c${CONNECTIONS} -d${DURATION}\` GET ${REQ_PATH} (Connection: close) |"
    echo "| Requests | ${wrk_requests} |"
    echo "| Requests/sec | ${wrk_rps} |"
    echo "| Latency p50 / p99 | ${wrk_p50} / ${wrk_p99} |"
    echo "| Socket errors | ${wrk_socket_errors} |"
    echo "| Non-2xx/3xx | ${wrk_non2xx} |"
    echo "| Server open fds before / after | ${fds_before:-n/a} / ${fds_after:-n/a} |"
    echo "| Server 2xx (metrics delta) | ${d2xx} |"
    echo "| Result | $([[ ${#failures[@]} -eq 0 ]] && echo PASS || echo "FAIL (${#failures[@]})") |"
} >"$OUT_DIR/summary.md"
cat "$OUT_DIR/summary.md"
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    cat "$OUT_DIR/summary.md" >>"$GITHUB_STEP_SUMMARY"
fi

if [[ ${#failures[@]} -ne 0 ]]; then
    echo >&2
    echo "${#failures[@]} check(s) failed:" >&2
    printf '  - %s\n' "${failures[@]}" >&2
    exit 1
fi
echo "All load-test checks passed."
