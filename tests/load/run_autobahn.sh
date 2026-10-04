#!/usr/bin/env bash
#
# Run the Autobahn|Testsuite WebSocket conformance suite (fuzzingclient mode)
# against bb-loadtest-server's /ws echo endpoint, in Docker.
#
# Conformance results are REPORT-ONLY for now: the script prints and uploads
# them but does not fail on a failing case. It does fail on infrastructure and
# robustness problems: Docker/Autobahn failing to run or produce a report, the
# server crashing, hanging on SIGTERM or exiting non-zero, or any sanitizer /
# leak report in the server log (so a run against the ASan build also exercises
# the WebSocket code under AddressSanitizer).
#
# Usage: tests/load/run_autobahn.sh [path-to-bb-loadtest-server]
#
# Environment (all optional):
#   BB_LOADTEST_BIN       server binary        (default build/tests/load/bb-loadtest-server)
#   BB_LOADTEST_PORT      port                 (default 8090)
#   BB_AUTOBAHN_OUT_DIR   results directory    (default build/autobahn-results)
#   BB_AUTOBAHN_IMAGE     Docker image         (default crossbario/autobahn-testsuite:0.8.2)
#   BB_AUTOBAHN_CASES     comma-separated case patterns   (default "*")
#   BB_AUTOBAHN_EXCLUDE   comma-separated exclusions      (default "9.*,12.*,13.*")
#   BB_AUTOBAHN_TIMEOUT   seconds before the container is killed (default 1800)
#
# Default exclusions: 12.* and 13.* test permessage-deflate compression, and 9.*
# are large-message performance/limit cases (up to 16 MiB; the server buffer
# limit is 4 MiB). Set BB_AUTOBAHN_EXCLUDE="" to run everything.

set -euo pipefail

SERVER_BIN="${1:-${BB_LOADTEST_BIN:-build/tests/load/bb-loadtest-server}}"
PORT="${BB_LOADTEST_PORT:-8090}"
OUT_DIR="${BB_AUTOBAHN_OUT_DIR:-build/autobahn-results}"
IMAGE="${BB_AUTOBAHN_IMAGE:-crossbario/autobahn-testsuite:0.8.2}"
CASES="${BB_AUTOBAHN_CASES:-*}"
EXCLUDE="${BB_AUTOBAHN_EXCLUDE-9.*,12.*,13.*}"
TIMEOUT="${BB_AUTOBAHN_TIMEOUT:-1800}"

BASE_URL="http://127.0.0.1:${PORT}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=tests/load/lib.sh
source "$HERE/lib.sh"

require_tools docker python3 curl awk
require_server_bin

mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
rm -rf "$OUT_DIR/config" "$OUT_DIR/reports" "$OUT_DIR/server.log" "$OUT_DIR/results.json" "$OUT_DIR/summary.md" 2>/dev/null || true
mkdir -p "$OUT_DIR/config" "$OUT_DIR/reports"

python3 "$HERE/autobahn/autobahn.py" config \
    --template "$HERE/autobahn/fuzzingclient.json" \
    --out "$OUT_DIR/config/fuzzingclient.json" \
    --url "ws://127.0.0.1:${PORT}/ws" \
    --cases "$CASES" \
    --exclude "$EXCLUDE"

start_server

# ------------------------------------------------------------ run Autobahn
# --network host so the container reaches the server on 127.0.0.1 (Linux).
echo "Running Autobahn ($IMAGE), cases: $CASES, excluding: ${EXCLUDE:-nothing}"
docker_rc=0
timeout "$TIMEOUT" docker run --rm --network host \
    -v "$OUT_DIR/config:/config:ro" \
    -v "$OUT_DIR/reports:/reports" \
    "$IMAGE" \
    wstest -m fuzzingclient -s /config/fuzzingclient.json || docker_rc=$?
[[ "$docker_rc" -eq 0 ]] || fail "Autobahn container exited with status $docker_rc"

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    fail "server process died during the Autobahn run"
fi

# ------------------------------------------------------------- stop server
stop_server

# ----------------------------------------------------------------- report
summary_rc=0
python3 "$HERE/autobahn/autobahn.py" summary \
    --index "$OUT_DIR/reports/servers/index.json" \
    --results "$OUT_DIR/results.json" \
    --markdown "$OUT_DIR/summary.md" || summary_rc=$?
[[ "$summary_rc" -eq 0 ]] || fail "no usable Autobahn report"

if [[ -f "$OUT_DIR/summary.md" ]]; then
    cat "$OUT_DIR/summary.md"
    if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
        cat "$OUT_DIR/summary.md" >>"$GITHUB_STEP_SUMMARY"
    fi
fi

if [[ ${#failures[@]} -ne 0 ]]; then
    echo >&2
    echo "${#failures[@]} check(s) failed:" >&2
    printf '  - %s\n' "${failures[@]}" >&2
    exit 1
fi
echo "Autobahn run completed (conformance results are report-only)."
