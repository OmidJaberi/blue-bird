#include "metrics.h"

#include <string.h>

static const uint32_t k_latency_bounds_ms[BB_METRICS_LATENCY_BOUNDS] = {
    1, 5, 10, 25, 50, 100, 250, 1000
};

uint32_t bb_metrics_latency_bound_ms(size_t i)
{
    return i < BB_METRICS_LATENCY_BOUNDS ? k_latency_bounds_ms[i] : UINT32_MAX;
}

size_t bb_metrics_latency_bucket_index(int64_t latency_ms)
{
    if (latency_ms < 0)
    {
        latency_ms = 0;
    }

    for (size_t i = 0; i < BB_METRICS_LATENCY_BOUNDS; i++)
    {
        if (latency_ms <= (int64_t) k_latency_bounds_ms[i])
        {
            return i;
        }
    }
    return BB_METRICS_LATENCY_BOUNDS; /* +Inf */
}

void bb_metrics_on_connection_accepted(bb_metrics_t *m)
{
    if (m)
    {
        m->connections_accepted_total++;
    }
}

void bb_metrics_on_ws_session_opened(bb_metrics_t *m)
{
    if (m)
    {
        m->ws_sessions_opened_total++;
    }
}

void bb_metrics_observe_request(bb_metrics_t *m, int status_code, int64_t latency_ms)
{
    if (!m)
    {
        return;
    }

    if (latency_ms < 0)
    {
        latency_ms = 0;
    }

    int cls = status_code / 100;
    if (status_code < 100 || cls > 5)
    {
        cls = 0;
    }

    m->requests_total++;
    m->responses_by_class[cls]++;
    m->latency_bucket[bb_metrics_latency_bucket_index(latency_ms)]++;
    m->latency_count++;
    m->latency_sum_ms += (uint64_t) latency_ms;
}

void bb_metrics_take_snapshot(const bb_metrics_t *m, size_t active_connections, size_t active_ws_sessions, bb_metrics_snapshot_t *out)
{
    if (!out)
    {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (m)
    {
        out->counters = *m;
    }
    out->active_connections = active_connections;
    out->active_ws_sessions = active_ws_sessions;
}
