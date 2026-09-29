#ifndef BB_WEB_METRICS_H
#define BB_WEB_METRICS_H

#include <stddef.h>
#include <stdint.h>

/*
 * Server metrics: plain counters plus a fixed-bucket latency histogram.
 *
 * Not thread-safe by design: the runtime drives the server from a single
 * event loop, so every update and every snapshot happens on that thread.
 * If metrics are ever read from another thread, switch the fields to atomics.
 *
 * Gauges (active connections, active WebSocket sessions) are NOT stored here.
 * They are derived from the connection/WebSocket lists at snapshot time so
 * they can never drift from the real state.
 */

/* Latency histogram: upper bounds in milliseconds, plus one +Inf bucket. */
#define BB_METRICS_LATENCY_BOUNDS 8
#define BB_METRICS_LATENCY_BUCKETS (BB_METRICS_LATENCY_BOUNDS + 1)

/* Indexes into responses_by_class: 1 -> 1xx ... 5 -> 5xx, 0 -> anything else. */
#define BB_METRICS_STATUS_CLASSES 6

typedef struct {
    uint64_t connections_accepted_total;
    uint64_t ws_sessions_opened_total;

    uint64_t requests_total;
    uint64_t responses_by_class[BB_METRICS_STATUS_CLASSES];

    /* Per-bucket (NOT cumulative) request counts; last bucket is +Inf. */
    uint64_t latency_bucket[BB_METRICS_LATENCY_BUCKETS];
    uint64_t latency_count;
    uint64_t latency_sum_ms;
} bb_metrics_t;

/* Point-in-time copy of the counters plus the derived gauges. */
typedef struct {
    bb_metrics_t counters;
    uint64_t active_connections;
    uint64_t active_ws_sessions;
} bb_metrics_snapshot_t;

/* Upper bound (inclusive, ms) of latency bucket i, i < BB_METRICS_LATENCY_BOUNDS. */
uint32_t bb_metrics_latency_bound_ms(size_t i);

/* Index of the bucket a latency (ms) falls into. Negative values count as 0. */
size_t bb_metrics_latency_bucket_index(int64_t latency_ms);

void bb_metrics_on_connection_accepted(bb_metrics_t *m);
void bb_metrics_on_ws_session_opened(bb_metrics_t *m);

/* Record one completed request: bumps requests_total, the status class and the latency histogram. */
void bb_metrics_observe_request(bb_metrics_t *m, int status_code, int64_t latency_ms);

/* Fill *out from the counters and the caller-supplied gauge values. */
void bb_metrics_take_snapshot(const bb_metrics_t *m, size_t active_connections, size_t active_ws_sessions, bb_metrics_snapshot_t *out);

#endif // BB_WEB_METRICS_H
