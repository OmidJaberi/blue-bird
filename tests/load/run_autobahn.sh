#!/usr/bin/env bash
#
# Run the Autobahn|Testsuite WebSocket conformance suite (fuzzingclient mode)
# against bb-loadtest-server's /ws echo endpoint, in Docker.
#
# Conformance is gated against a committed baseline (autobahn/expected-results.json):
# a case that got WORSE than its baseline status fails the run; known failures do
# not, and improvements are reported so the baseline can be refreshed. It also
# fails on infrastructure and robustness problems: Docker/Autobahn failing to run
# or produce a report, the server crashing, hanging on SIGTERM or exiting
# non-zero, or any sanitizer / leak report in the server log (so a run against
# the ASan build also exercises the WebSocket code under AddressSanitizer).
#
# Usage: tests/load/run_autobahn.sh [path-to-bb-loadtest-server]
#
# Environment (all optional):
#   BB_LOADTEST_BIN       server binary        (default build/tests/load/bb-loadtest-server)
#   BB_LOADTEST_PORT      port                 (default 8090)
#   BB_AUTOBAHN_OUT_DIR   results directory    (default build/autobahn-results)
#   BB_AUTOBAHN_IMAGE     Docker image         (default: crossbario/autobahn-testsuite pinned by digest)
#   BB_AUTOBAHN_CASES     comma-separated case patterns   (default "*")
#   BB_AUTOBAHN_EXCLUDE   comma-separated exclusions      (default "9.*,12.*,13.*")
#   BB_AUTOBAHN_TIMEOUT   seconds before the container is killed (default 1800)
#   BB_AUTOBAHN_EXPECTED  baseline file        (default tests/load/autobahn/expected-results.json)
#   BB_AUTOBAHN_ENFORCE   1 = fail on regressions against the baseline, 0 = report only
#                         (default 1 for the default case selection, else 0)
#   BB_AUTOBAHN_UPDATE_EXPECTED=1   rewrite the baseline from this run (then review the diff)
#
# Default exclusions: 12.* and 13.* test permessage-deflate compression, and 9.*
# are large-message performance/limit cases (up to 16 MiB; the server buffer
# limit is 4 MiB). Set BB_AUTOBAHN_EXCLUDE="" to run everything.

set -euo pipefail

SERVER_BIN="${1:-${BB_LOADTEST_BIN:-build/tests/load/bb-loadtest-server}}"
PORT="${BB_LOADTEST_PORT:-8090}"
OUT_DIR="${BB_AUTOBAHN_OUT_DIR:-build/autobahn-results}"
# Pinned so a new image can't silently add or change cases under the baseline.
# To move to a newer image, pull it, take the digest from the "Image:" line of the
# run summary, and refresh the baseline.
IMAGE="${BB_AUTOBAHN_IMAGE:-crossbario/autobahn-testsuite@sha256:519915fb568b04c9383f70a1c405ae3ff44ab9e35835b085239c258b6fac3074}"
CASES="${BB_AUTOBAHN_CASES:-*}"
DEFAULT_EXCLUDE="9.*,12.*,13.*"
EXCLUDE="${BB_AUTOBAHN_EXCLUDE-$DEFAULT_EXCLUDE}"
TIMEOUT="${BB_AUTOBAHN_TIMEOUT:-1800}"

BASE_URL="http://127.0.0.1:${PORT}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EXPECTED="${BB_AUTOBAHN_EXPECTED:-$HERE/autobahn/expected-results.json}"

# The baseline describes the default case selection, so enforce it only for that
# (a subset or a different exclusion list would show missing or unknown cases).
if [[ "$CASES" == "*" && "$EXCLUDE" == "$DEFAULT_EXCLUDE" ]]; then
    default_selection=1
else
    default_selection=0
fi
ENFORCE="${BB_AUTOBAHN_ENFORCE:-$default_selection}"

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

# Pull before starting the server so a bad image name fails fast and clearly.
# The resolved digest is printed (and added to the summary) so a known-good
# image can be pinned later with BB_AUTOBAHN_IMAGE=repo@sha256:...
echo "Pulling $IMAGE"
if ! docker pull "$IMAGE"; then
    echo "error: could not pull Docker image '$IMAGE'" >&2
    exit 1
fi
IMAGE_DIGEST="$(docker image inspect --format '{{index .RepoDigests 0}}' "$IMAGE" 2>/dev/null || true)"
echo "Image: ${IMAGE_DIGEST:-$IMAGE}"

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
summary_args=(
    --index "$OUT_DIR/reports/servers/index.json"
    --results "$OUT_DIR/results.json"
    --markdown "$OUT_DIR/summary.md"
)
if [[ "$ENFORCE" == "1" || "${BB_AUTOBAHN_UPDATE_EXPECTED:-0}" == "1" ]]; then
    if [[ -f "$EXPECTED" ]]; then
        summary_args+=(--expected "$EXPECTED")
        [[ "$default_selection" -eq 1 ]] && summary_args+=(--require-complete)
        [[ "${BB_AUTOBAHN_UPDATE_EXPECTED:-0}" == "1" ]] && summary_args+=(--update-expected)
    else
        fail "baseline file not found: $EXPECTED"
    fi
fi

summary_rc=0
python3 "$HERE/autobahn/autobahn.py" summary "${summary_args[@]}" || summary_rc=$?
case "$summary_rc" in
    0) ;;
    3) fail "Autobahn conformance regressed against the baseline (see the summary above)" ;;
    *) fail "no usable Autobahn report" ;;
esac

if [[ -f "$OUT_DIR/summary.md" ]]; then
    printf '\nImage: `%s`\n' "${IMAGE_DIGEST:-$IMAGE}" >>"$OUT_DIR/summary.md"
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
if [[ "$ENFORCE" == "1" ]]; then
    echo "Autobahn run completed; no conformance regressions against the baseline."
else
    echo "Autobahn run completed (conformance was not enforced for this run)."
fi
