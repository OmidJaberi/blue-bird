# Metrics

The web server keeps a small set of built-in counters and gauges (connections, WebSocket sessions, requests by status class and a request latency histogram). You can read them in code, or expose them in the [Prometheus](https://prometheus.io) text format on an HTTP route.

Metrics collection is always on and costs a few integer increments per request. **Nothing is exposed over HTTP until you call `bb_server_enable_metrics()`.**

---

# Quick Start

```c
#include <blue-bird/web/server.h>

bb_server_t *server = bb_server_create(8080);

bb_server_add_route(server, "GET", "/", root_handler);

if (bb_server_enable_metrics(server, NULL) != 0)
{
    // invalid path, route could not be registered, or already enabled
}

bb_server_start(server);
bb_runtime_run_default();
```

```bash
curl http://localhost:8080/metrics
```

---

# Exposing Metrics over HTTP

```c
int bb_server_enable_metrics(bb_server_t *server, const char *path);
```

Registers `GET <path>` and serves the current metrics in Prometheus text exposition format (version `0.0.4`, `Content-Type: text/plain; version=0.0.4; charset=utf-8`).

- `path` may be `NULL`, which means `"/metrics"`.
- Returns `0` on success and `-1` if `server` is `NULL`, `path` does not start with `/`, the route could not be registered, or metrics were already enabled. Call it once per server.

The metrics route is an ordinary route, so the usual router and middleware rules apply:

- The first route registered for a path wins. A route you added earlier on the same path shadows the metrics route.
- Pre and post [middleware](middleware.md) run around it, so authentication middleware protects it like any other route.

Metrics can reveal traffic volume and error rates. If the server is reachable from untrusted networks, protect the endpoint with middleware, a different path, or network-level rules.

---

# Reading Metrics in Code

```c
int bb_server_get_metrics(const bb_server_t *server, bb_metrics_snapshot_t *out);
```

Copies the current metrics into `*out`. Returns `0` on success and `-1` if `server` or `out` is `NULL`. This works whether or not `bb_server_enable_metrics()` was called.

```c
bb_metrics_snapshot_t snap;

if (bb_server_get_metrics(server, &snap) == 0)
{
    printf("requests: %llu, active connections: %llu\n",
           (unsigned long long) snap.counters.requests_total,
           (unsigned long long) snap.active_connections);
}
```

**Threading:** metrics are not thread-safe. The runtime drives the server from a single event loop, so call `bb_server_get_metrics()` from the runtime thread (inside a route handler or a runtime task) or while the runtime is stopped.

## Snapshot Structure

Declared in `blue-bird/web/metrics.h` (included by `server.h`).

```c
typedef struct {
    bb_metrics_t counters;
    uint64_t active_connections;
    uint64_t active_ws_sessions;
} bb_metrics_snapshot_t;
```

| Field | Kind | Meaning |
|---|---|---|
| `active_connections` | gauge | Open HTTP connections. Connections upgraded to WebSocket are not counted. |
| `active_ws_sessions` | gauge | Open WebSocket sessions. |
| `counters` | counters | See below. |

The gauges are derived from the live connection and WebSocket lists when the snapshot is taken, so they cannot drift from the real state.

```c
typedef struct {
    uint64_t connections_accepted_total;
    uint64_t ws_sessions_opened_total;

    uint64_t requests_total;
    uint64_t responses_by_class[BB_METRICS_STATUS_CLASSES];

    uint64_t latency_bucket[BB_METRICS_LATENCY_BUCKETS];
    uint64_t latency_count;
    uint64_t latency_sum_ms;
} bb_metrics_t;
```

Counters only increase from server creation onward.

- `responses_by_class[i]`: index `1` is `1xx`, up to `5` for `5xx`. Index `0` is anything else.
- `latency_bucket[i]`: per-bucket request counts, **not cumulative**. The last bucket is `+Inf`.
- `latency_count` / `latency_sum_ms`: number of observed requests and the sum of their latencies in milliseconds.

## Latency Buckets

```c
uint32_t bb_metrics_latency_bound_ms(size_t i);
```

Returns the inclusive upper bound, in milliseconds, of latency bucket `i`. For `i >= BB_METRICS_LATENCY_BOUNDS` (the `+Inf` bucket) it returns `UINT32_MAX`.

| Bucket | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| Upper bound | 1 ms | 5 ms | 10 ms | 25 ms | 50 ms | 100 ms | 250 ms | 1000 ms | +Inf |

Constants: `BB_METRICS_LATENCY_BOUNDS` (8), `BB_METRICS_LATENCY_BUCKETS` (9) and `BB_METRICS_STATUS_CLASSES` (6).

---

# Prometheus Output

| Metric | Type | Description |
|---|---|---|
| `bluebird_http_connections_active` | gauge | Open HTTP connections (connections upgraded to WebSocket are not counted). |
| `bluebird_websocket_sessions_active` | gauge | Open WebSocket sessions. |
| `bluebird_connections_accepted_total` | counter | TCP connections accepted since server start. |
| `bluebird_websocket_sessions_opened_total` | counter | WebSocket sessions opened since server start. |
| `bluebird_http_requests_total{status_class="..."}` | counter | HTTP requests answered, by response status class: `1xx`, `2xx`, `3xx`, `4xx`, `5xx`, `other`. |
| `bluebird_http_request_duration_seconds` | histogram | Request latency: `_bucket{le="..."}` (cumulative), `_sum` and `_count`. |

Example output (abridged):

```txt
# HELP bluebird_http_connections_active Open HTTP connections (connections upgraded to WebSocket are not counted).
# TYPE bluebird_http_connections_active gauge
bluebird_http_connections_active 1
# HELP bluebird_http_requests_total HTTP requests answered, by response status class.
# TYPE bluebird_http_requests_total counter
bluebird_http_requests_total{status_class="1xx"} 0
bluebird_http_requests_total{status_class="2xx"} 42
bluebird_http_requests_total{status_class="3xx"} 0
bluebird_http_requests_total{status_class="4xx"} 3
bluebird_http_requests_total{status_class="5xx"} 0
bluebird_http_requests_total{status_class="other"} 0
# TYPE bluebird_http_request_duration_seconds histogram
bluebird_http_request_duration_seconds_bucket{le="0.001"} 40
bluebird_http_request_duration_seconds_bucket{le="0.005"} 44
...
bluebird_http_request_duration_seconds_bucket{le="1"} 45
bluebird_http_request_duration_seconds_bucket{le="+Inf"} 45
bluebird_http_request_duration_seconds_sum 0.052
bluebird_http_request_duration_seconds_count 45
```

## Notes on What Is Measured

- **Latency** is measured from the first read of a request until its response has been built and queued for writing. It does not include the socket write, so it reflects server-side handling time, not what a client observes over the network.
- **WebSocket upgrades** are counted as one request with status `101` (class `1xx`). Frames sent afterwards are not requests and are not part of the HTTP counters.
- **Requests rejected by middleware** are still answered, so they are counted in `bluebird_http_requests_total` with their resulting status class.
- **A scrape sees the state from just before it finishes.** The scrape request is not counted in the request totals of its own response, but its connection is in `bluebird_http_connections_active`.
- The server closes the connection after every HTTP response, so each request is a new connection and `bluebird_connections_accepted_total` grows with request volume.
- Numbers in `le` labels and `_sum` are formatted without locale-dependent separators, so output is identical regardless of the C locale.

---

# Rendering Metrics Yourself

The Prometheus renderer is an internal function (`bb_metrics_render_prometheus()`), used by the built-in route. To publish metrics in a different format or on a custom route, read a snapshot with `bb_server_get_metrics()` from a handler and format the fields yourself:

```c
bb_error_t stats_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;

    bb_metrics_snapshot_t snap;
    // `server` here is your application's own pointer to the bb_server_t
    if (bb_server_get_metrics(server, &snap) != 0)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to read metrics.");
    }

    char body[128];
    snprintf(body, sizeof(body), "requests=%llu active=%llu\n",
             (unsigned long long) snap.counters.requests_total,
             (unsigned long long) snap.active_connections);

    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, body);

    return BB_SUCCESS();
}
```

---

# Prometheus Configuration

```yaml
scrape_configs:
  - job_name: blue-bird
    metrics_path: /metrics
    static_configs:
      - targets: ["localhost:8080"]
```

Useful queries:

```txt
# requests per second
sum(rate(bluebird_http_requests_total[1m]))

# share of 5xx responses
sum(rate(bluebird_http_requests_total{status_class="5xx"}[5m]))
  / sum(rate(bluebird_http_requests_total[5m]))

# p95 request latency
histogram_quantile(0.95,
  sum by (le) (rate(bluebird_http_request_duration_seconds_bucket[5m])))
```

---

# Load Testing

The `bb-loadtest-server` in `tests/load/` enables metrics at `GET /metrics`, and `tests/load/run_wrk.sh` cross-checks `wrk`'s results against the server's own counters. See [tests/load/README.md](../../tests/load/README.md).

---

# Related Documentation

- [overview.md](overview.md)
- [middleware.md](middleware.md)
- [routing.md](routing.md)
