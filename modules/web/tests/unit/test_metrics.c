#include "metrics.h"
#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
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

static void test_render_prometheus(void)
{
    printf("Testing metrics prometheus render...\n");
    bb_metrics_t m;
    memset(&m, 0, sizeof(m));
    bb_metrics_on_connection_accepted(&m);
    bb_metrics_on_ws_session_opened(&m);
    bb_metrics_observe_request(&m, 200, 0);
    bb_metrics_observe_request(&m, 200, 3);
    bb_metrics_observe_request(&m, 404, 30);
    bb_metrics_observe_request(&m, 500, 5000);

    bb_metrics_snapshot_t snap;
    bb_metrics_take_snapshot(&m, 7, 3, &snap);

    size_t need = bb_metrics_render_prometheus(&snap, NULL, 0);
    BB_ASSERT(need > 0);

    char *buf = malloc(need + 1);
    BB_ASSERT(buf != NULL);
    BB_ASSERT(bb_metrics_render_prometheus(&snap, buf, need + 1) == need);
    BB_ASSERT(strlen(buf) == need);

    BB_ASSERT(strstr(buf, "# TYPE bluebird_http_connections_active gauge\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_http_connections_active 7\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_websocket_sessions_active 3\n") != NULL);
    BB_ASSERT(strstr(buf, "# TYPE bluebird_connections_accepted_total counter\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_connections_accepted_total 1\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_websocket_sessions_opened_total 1\n") != NULL);

    BB_ASSERT(strstr(buf, "bluebird_http_requests_total{status_class=\"2xx\"} 2\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_http_requests_total{status_class=\"4xx\"} 1\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_http_requests_total{status_class=\"5xx\"} 1\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_http_requests_total{status_class=\"3xx\"} 0\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_http_requests_total{status_class=\"other\"} 0\n") != NULL);

    /* Histogram: cumulative buckets, +Inf == count, sum in seconds. */
    BB_ASSERT(strstr(buf, "# TYPE bluebird_http_request_duration_seconds histogram\n") != NULL);
    BB_ASSERT(strstr(buf, "_bucket{le=\"0.001\"} 1\n") != NULL);
    BB_ASSERT(strstr(buf, "_bucket{le=\"0.005\"} 2\n") != NULL);
    BB_ASSERT(strstr(buf, "_bucket{le=\"0.01\"} 2\n") != NULL);
    BB_ASSERT(strstr(buf, "_bucket{le=\"0.05\"} 3\n") != NULL);
    BB_ASSERT(strstr(buf, "_bucket{le=\"1\"} 3\n") != NULL);
    BB_ASSERT(strstr(buf, "_bucket{le=\"+Inf\"} 4\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_http_request_duration_seconds_sum 5.033\n") != NULL);
    BB_ASSERT(strstr(buf, "bluebird_http_request_duration_seconds_count 4\n") != NULL);

    free(buf);
}

static void test_render_seconds_format(void)
{
    printf("Testing metrics prometheus seconds formatting...\n");
    static const struct { uint64_t sum_ms; const char *expect; } cases[] = {
        { 0, "_sum 0\n" }, { 10, "_sum 0.01\n" }, { 1500, "_sum 1.5\n" },
        { 2000, "_sum 2\n" }, { 1234567, "_sum 1234.567\n" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bb_metrics_t m;
        memset(&m, 0, sizeof(m));
        m.latency_sum_ms = cases[i].sum_ms;

        bb_metrics_snapshot_t snap;
        bb_metrics_take_snapshot(&m, 0, 0, &snap);

        char buf[4096];
        size_t n = bb_metrics_render_prometheus(&snap, buf, sizeof(buf));
        BB_ASSERT(n > 0 && n < sizeof(buf));
        BB_ASSERT(strstr(buf, cases[i].expect) != NULL);
    }
}

static void test_render_truncation_and_null(void)
{
    printf("Testing metrics prometheus render truncation and NULL...\n");
    bb_metrics_snapshot_t snap;
    bb_metrics_take_snapshot(NULL, 1, 1, &snap);

    size_t need = bb_metrics_render_prometheus(&snap, NULL, 0);

    /* Too-small buffer: reports the full size, stays NUL-terminated, never overruns. */
    char small[16];
    memset(small, 'x', sizeof(small));
    BB_ASSERT(bb_metrics_render_prometheus(&snap, small, sizeof(small)) == need);
    BB_ASSERT(strlen(small) == sizeof(small) - 1);

    char one[1] = { 'x' };
    BB_ASSERT(bb_metrics_render_prometheus(&snap, one, sizeof(one)) == need);
    BB_ASSERT(one[0] == '\0');

    char buf[8] = "junk";
    BB_ASSERT(bb_metrics_render_prometheus(NULL, buf, sizeof(buf)) == 0);
    BB_ASSERT(buf[0] == '\0');
    BB_ASSERT(bb_metrics_render_prometheus(NULL, NULL, 0) == 0);
}

int main(void)
{
    test_bucket_index();
    test_observe_request();
    test_counters_and_snapshot();
    test_null_safety();
    test_render_prometheus();
    test_render_seconds_format();
    test_render_truncation_and_null();
    printf("All metrics tests passed.\n");
    return 0;
}
