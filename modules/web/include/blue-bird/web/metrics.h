#ifndef BB_METRICS_H
#define BB_METRICS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>

/*
 * Server metrics, read through bb_server_get_metrics() (see server.h).
 *
 * Counters are monotonic since server creation. The two gauges (active
 * connections, active WebSocket sessions) are point-in-time values.
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

/*
 * Upper bound (inclusive, ms) of latency bucket i for i < BB_METRICS_LATENCY_BOUNDS.
 * Returns UINT32_MAX for the +Inf bucket (i >= BB_METRICS_LATENCY_BOUNDS).
 */
uint32_t bb_metrics_latency_bound_ms(size_t i);


#ifdef __cplusplus
}
#endif

#endif // BB_METRICS_H
