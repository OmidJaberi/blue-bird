# shellcheck shell=bash
#
# Helpers shared by the load-test scripts (run_wrk.sh, run_autobahn.sh).
# Source this file; do not run it. The caller must set SERVER_BIN, PORT, OUT_DIR
# and BASE_URL before calling start_server.
#
# Optional environment:
#   BB_SERVER_WRAPPER    command prefix for the server, e.g. "valgrind --leak-check=full ..."
#   BB_START_TIMEOUT_S   seconds to wait for the server to answer   (default 10)
#   BB_STOP_TIMEOUT_S    seconds to wait for exit after SIGTERM     (default 10)

failures=()
fail() { failures+=("$1"); echo "FAIL: $1" >&2; }

# require_tools tool...   (exit 2 if one is missing)
require_tools() {
    local tool
    for tool in "$@"; do
        command -v "$tool" >/dev/null 2>&1 || { echo "error: '$tool' not found in PATH" >&2; exit 2; }
    done
}

require_server_bin() {
    [[ -x "$SERVER_BIN" ]] || { echo "error: server binary not found or not executable: $SERVER_BIN" >&2; exit 2; }
}

SERVER_PID=""
cleanup() {
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -KILL "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

scrape() { curl -fsS --max-time 5 "${BASE_URL}/metrics" -o "$1"; }

# Open file descriptors of a process (Linux /proc only; empty elsewhere).
fd_count() { [[ -d "/proc/$1/fd" ]] && ls "/proc/$1/fd" 2>/dev/null | wc -l | tr -d " "; }

# metric <file> <exact series text, e.g. bluebird_http_connections_active>  -> value
metric() { awk -v key="$2" 'index($0, key " ") == 1 { print $NF; exit }' "$1"; }

# Starts the server in the background, sets SERVER_PID, waits until GET / answers.
# Exits the calling script if the server dies or never becomes ready.
start_server() {
    # BB_SERVER_WRAPPER is split on whitespace on purpose. A wrapper such as
    # valgrind runs the server in the same process, so SERVER_PID (and the fd
    # count read from /proc) is the wrapper's.
    local -a cmd=()
    if [[ -n "${BB_SERVER_WRAPPER:-}" ]]; then
        read -r -a cmd <<<"$BB_SERVER_WRAPPER"
    fi
    cmd+=("$SERVER_BIN" "$PORT")

    echo "Starting ${cmd[*]}"
    "${cmd[@]}" >"$OUT_DIR/server.log" 2>&1 &
    SERVER_PID=$!

    local ready=0 _
    local tries=$(( ${BB_START_TIMEOUT_S:-10} * 10 ))
    for _ in $(seq 1 "$tries"); do
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            echo "error: server exited during startup; log:" >&2
            cat "$OUT_DIR/server.log" >&2
            SERVER_PID=""
            exit 1
        fi
        if curl -fsS --max-time 1 -o /dev/null "${BASE_URL}/" 2>/dev/null; then
            ready=1
            break
        fi
        sleep 0.1
    done
    if [[ "$ready" -ne 1 ]]; then
        echo "error: server did not become ready within ${BB_START_TIMEOUT_S:-10}s" >&2
        exit 1
    fi
}

# Stops the server with SIGTERM and records a failure if it hangs, exits non-zero
# (LeakSanitizer exits non-zero on leaks) or left a sanitizer report in its log.
stop_server() {
    kill -TERM "$SERVER_PID" 2>/dev/null || true
    local _
    local tries=$(( ${BB_STOP_TIMEOUT_S:-10} * 10 ))
    for _ in $(seq 1 "$tries"); do
        kill -0 "$SERVER_PID" 2>/dev/null || break
        sleep 0.1
    done

    local server_rc=0
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        fail "server did not exit within ${BB_STOP_TIMEOUT_S:-10}s of SIGTERM"
        kill -KILL "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    else
        wait "$SERVER_PID" || server_rc=$?
        [[ "$server_rc" -eq 0 ]] || fail "server exited with status $server_rc after SIGTERM"
    fi
    SERVER_PID=""

    if grep -E -q "AddressSanitizer|LeakSanitizer|runtime error:|ThreadSanitizer" "$OUT_DIR/server.log"; then
        fail "sanitizer report in $OUT_DIR/server.log"
    fi

    # Valgrind (when used as the wrapper) writes its report to --log-file, if given.
    if [[ -f "$OUT_DIR/valgrind.log" ]] && grep -E -q "ERROR SUMMARY: [1-9]" "$OUT_DIR/valgrind.log"; then
        fail "Valgrind reported errors, see $OUT_DIR/valgrind.log"
    fi
}
