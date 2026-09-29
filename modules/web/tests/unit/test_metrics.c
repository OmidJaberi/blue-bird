#include "metrics.h"
#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

static void test_bucket_index(void)
{
    printf("Testing metrics latency bucket index...\n");
    BB_ASSERT(bb_metrics_latency_bucket_index(-5) == 0);   /* clamped */
    BB_ASSERT(bb_metrics_latency_bucket_index(0) == 0);
    BB_ASSERT(bb_metrics_latency_bucket_index(1) == 0);    /* inclusive bound */
    BB_ASSERT(bb_metrics_latency_bucket_index(2) == 1);
    BB_ASSERT(bb_metrics_latency_bucket_index(5) == 1);
    BB_ASSERT(bb_metrics_latency_bucket_index(6) == 2);
    BB_ASSERT(bb_metrics_latency_bucket_index(1000) == BB_METRICS_LATENCY_BOUNDS - 1);
    BB_ASSERT(bb_metrics_latency_bucket_index(1001) == BB_METRICS_LATENCY_BOUNDS); /* +Inf */
    BB_ASSERT(bb_metrics_latency_bucket_index(INT64_MAX) == BB_METRICS_LATENCY_BOUNDS);

    BB_ASSERT(bb_metrics_latency_bound_ms(0) == 1);
    BB_ASSERT(bb_metrics_latency_bound_ms(BB_METRICS_LATENCY_BOUNDS - 1) == 1000);
    BB_ASSERT(bb_metrics_latency_bound_ms(BB_METRICS_LATENCY_BOUNDS) == UINT32_MAX);
}

static void test_observe_request(void)
{
    printf("Testing metrics observe_request...\n");
    bb_metrics_t m;
    memset(&m, 0, sizeof(m));

    bb_metrics_observe_request(&m, 200, 0);
    bb_metrics_observe_request(&m, 204, 3);
    bb_metrics_observe_request(&m, 404, 30);
    bb_metrics_observe_request(&m, 500, 5000);
    bb_metrics_observe_request(&m, 0, -1);    /* invalid status, clamped latency */
    bb_metrics_observe_request(&m, 999, 1);   /* out-of-range status class */

    BB_ASSERT(m.requests_total == 6);
    BB_ASSERT(m.responses_by_class[2] == 2);
    BB_ASSERT(m.responses_by_class[4] == 1);
    BB_ASSERT(m.responses_by_class[5] == 1);
    BB_ASSERT(m.responses_by_class[0] == 2);

    BB_ASSERT(m.latency_count == 6);
    BB_ASSERT(m.latency_sum_ms == 0 + 3 + 30 + 5000 + 0 + 1);

    uint64_t total = 0;
    for (size_t i = 0; i < BB_METRICS_LATENCY_BUCKETS; i++)
    {
        total += m.latency_bucket[i];
    }
    BB_ASSERT(total == m.latency_count); /* every observation lands in exactly one bucket */
    BB_ASSERT(m.latency_bucket[BB_METRICS_LATENCY_BOUNDS] == 1); /* the 5000 ms one */
}

static void test_counters_and_snapshot(void)
{
    printf("Testing metrics counters and snapshot...\n");
    bb_metrics_t m;
    memset(&m, 0, sizeof(m));

    bb_metrics_on_connection_accepted(&m);
    bb_metrics_on_connection_accepted(&m);
    bb_metrics_on_ws_session_opened(&m);
    bb_metrics_observe_request(&m, 200, 2);

    bb_metrics_snapshot_t snap;
    bb_metrics_take_snapshot(&m, 7, 3, &snap);
    BB_ASSERT(snap.active_connections == 7);
    BB_ASSERT(snap.active_ws_sessions == 3);
    BB_ASSERT(snap.counters.connections_accepted_total == 2);
    BB_ASSERT(snap.counters.ws_sessions_opened_total == 1);
    BB_ASSERT(snap.counters.requests_total == 1);

    /* The snapshot is a copy: later updates must not change it. */
    bb_metrics_on_connection_accepted(&m);
    BB_ASSERT(snap.counters.connections_accepted_total == 2);
}

static void test_null_safety(void)
{
    printf("Testing metrics NULL safety...\n");
    bb_metrics_on_connection_accepted(NULL);
    bb_metrics_on_ws_session_opened(NULL);
    bb_metrics_observe_request(NULL, 200, 1);
    bb_metrics_take_snapshot(NULL, 1, 1, NULL);

    bb_metrics_snapshot_t snap;
    memset(&snap, 0xFF, sizeof(snap));
    bb_metrics_take_snapshot(NULL, 4, 2, &snap);
    BB_ASSERT(snap.active_connections == 4);
    BB_ASSERT(snap.active_ws_sessions == 2);
    BB_ASSERT(snap.counters.requests_total == 0);
}

int main(void)
{
    test_bucket_index();
    test_observe_request();
    test_counters_and_snapshot();
    test_null_safety();
    printf("All metrics tests passed.\n");
    return 0;
}
