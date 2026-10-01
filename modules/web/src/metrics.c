#include "metrics.h"

#include <stdarg.h>
#include <stdio.h>
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

/* ------------------------------------------------------------------ */
/* Prometheus text exposition                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    char *buf;
    size_t cap;
    size_t len; /* bytes the full output needs so far, even past cap */
} bb_out_t;

static void out_printf(bb_out_t *o, const char *fmt, ...)
{
    char *dst = NULL;
    size_t room = 0;
    if (o->buf && o->len < o->cap)
    {
        dst = o->buf + o->len;
        room = o->cap - o->len;
    }

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(dst, room, fmt, ap);
    va_end(ap);

    if (n > 0)
    {
        o->len += (size_t) n;
    }
}

/*
 * Milliseconds -> seconds as a decimal string ("0.005", "0.25", "1", "5.001").
 * Integer math only, so the output never depends on the C locale's decimal
 * separator, and trailing zeros are trimmed for conventional "le" labels.
 */
static void fmt_seconds(char out[32], uint64_t ms)
{
    unsigned long long whole = (unsigned long long) (ms / 1000);
    unsigned long long frac = (unsigned long long) (ms % 1000);

    if (frac == 0)
    {
        snprintf(out, 32, "%llu", whole);
        return;
    }

    char digits[4];
    snprintf(digits, sizeof(digits), "%03llu", frac);
    for (int i = 2; i >= 0 && digits[i] == '0'; i--)
    {
        digits[i] = '\0';
    }
    snprintf(out, 32, "%llu.%s", whole, digits);
}

static void out_metric_header(bb_out_t *o, const char *name, const char *type, const char *help)
{
    out_printf(o, "# HELP %s %s\n# TYPE %s %s\n", name, help, name, type);
}

size_t bb_metrics_render_prometheus(const bb_metrics_snapshot_t *snap, char *buf, size_t cap)
{
    if (!snap)
    {
        if (buf && cap > 0)
        {
            buf[0] = '\0';
        }
        return 0;
    }

    bb_out_t o = { buf, cap, 0 };
    if (buf && cap > 0)
    {
        buf[0] = '\0';
    }

    const bb_metrics_t *m = &snap->counters;

    out_metric_header(&o, "bluebird_http_connections_active", "gauge",
                      "Open HTTP connections (connections upgraded to WebSocket are not counted).");
    out_printf(&o, "bluebird_http_connections_active %llu\n", (unsigned long long) snap->active_connections);

    out_metric_header(&o, "bluebird_websocket_sessions_active", "gauge",
                      "Open WebSocket sessions.");
    out_printf(&o, "bluebird_websocket_sessions_active %llu\n", (unsigned long long) snap->active_ws_sessions);

    out_metric_header(&o, "bluebird_connections_accepted_total", "counter",
                      "TCP connections accepted since server start.");
    out_printf(&o, "bluebird_connections_accepted_total %llu\n", (unsigned long long) m->connections_accepted_total);

    out_metric_header(&o, "bluebird_websocket_sessions_opened_total", "counter",
                      "WebSocket sessions opened since server start.");
    out_printf(&o, "bluebird_websocket_sessions_opened_total %llu\n", (unsigned long long) m->ws_sessions_opened_total);

    out_metric_header(&o, "bluebird_http_requests_total", "counter",
                      "HTTP requests answered, by response status class.");
    static const char *const k_class_labels[BB_METRICS_STATUS_CLASSES] = {
        "other", "1xx", "2xx", "3xx", "4xx", "5xx"
    };
    for (size_t i = 1; i <= BB_METRICS_STATUS_CLASSES; i++)
    {
        size_t cls = i % BB_METRICS_STATUS_CLASSES; /* 1xx..5xx first, "other" last */
        out_printf(&o, "bluebird_http_requests_total{status_class=\"%s\"} %llu\n",
                   k_class_labels[cls], (unsigned long long) m->responses_by_class[cls]);
    }

    out_metric_header(&o, "bluebird_http_request_duration_seconds", "histogram",
                      "Time from the first read of a request to its response being queued (excludes the socket write).");
    uint64_t cumulative = 0;
    for (size_t i = 0; i < BB_METRICS_LATENCY_BUCKETS; i++)
    {
        cumulative += m->latency_bucket[i];
        if (i < BB_METRICS_LATENCY_BOUNDS)
        {
            char le[32];
            fmt_seconds(le, bb_metrics_latency_bound_ms(i));
            out_printf(&o, "bluebird_http_request_duration_seconds_bucket{le=\"%s\"} %llu\n",
                       le, (unsigned long long) cumulative);
        }
        else
        {
            out_printf(&o, "bluebird_http_request_duration_seconds_bucket{le=\"+Inf\"} %llu\n",
                       (unsigned long long) cumulative);
        }
    }
    char sum[32];
    fmt_seconds(sum, m->latency_sum_ms);
    out_printf(&o, "bluebird_http_request_duration_seconds_sum %s\n", sum);
    out_printf(&o, "bluebird_http_request_duration_seconds_count %llu\n", (unsigned long long) m->latency_count);

    return o.len;
}
