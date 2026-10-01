#ifndef BB_WEB_METRICS_H
#define BB_WEB_METRICS_H

#include "blue-bird/web/metrics.h"

/*
 * Internal mutators for the public bb_metrics_t (see blue-bird/web/metrics.h).
 *
 * Not thread-safe by design: the runtime drives the server from a single
 * event loop, so every update and every snapshot happens on that thread.
 * If metrics are ever read from another thread, switch the fields to atomics.
 *
 * Gauges (active connections, active WebSocket sessions) are NOT stored in
 * bb_metrics_t. They are derived from the connection/WebSocket lists at
 * snapshot time so they can never drift from the real state.
 */

/* Index of the bucket a latency (ms) falls into. Negative values count as 0. */
size_t bb_metrics_latency_bucket_index(int64_t latency_ms);

void bb_metrics_on_connection_accepted(bb_metrics_t *m);
void bb_metrics_on_ws_session_opened(bb_metrics_t *m);

/* Record one completed request: bumps requests_total, the status class and the latency histogram. */
void bb_metrics_observe_request(bb_metrics_t *m, int status_code, int64_t latency_ms);

/* Fill *out from the counters and the caller-supplied gauge values. */
void bb_metrics_take_snapshot(const bb_metrics_t *m, size_t active_connections, size_t active_ws_sessions, bb_metrics_snapshot_t *out);

/*
 * Render a snapshot in the Prometheus text exposition format (version 0.0.4).
 *
 * snprintf-style: returns the number of bytes the full output needs, excluding
 * the terminating NUL, and writes at most cap bytes (always NUL-terminated when
 * cap > 0). Call with (snap, NULL, 0) to size a buffer, then again to fill it.
 * Returns 0 if snap is NULL.
 */
size_t bb_metrics_render_prometheus(const bb_metrics_snapshot_t *snap, char *buf, size_t cap);

#endif // BB_WEB_METRICS_H
