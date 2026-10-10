# Metrics

The web server keeps a small set of built-in counters and gauges (connections, WebSocket sessions, requests by status class, and a request latency histogram). You can read them in code or expose them in the [Prometheus text exposition format](https://prometheus.io/docs/instrumenting/exposition_formats/) on an HTTP route.

Metrics collection is always on and costs a few integer increments per request. **Nothing is exposed over HTTP until you call `bb_server_enable_metrics()`.**

## Quick Start

```c
#include <blue-bird/web/server.h>

bb_server_t *server = bb_server_create(8080);

bb_server_add_route(server, "GET", "/", root_handler);

if (bb_server_enable_metrics(server, NULL) != 0)
{
    // Invalid path, route could not be registered, or already enabled.
}

bb_server_start(server);
bb_runtime_run_default();
```

Scrape the endpoint:

```bash
curl http://localhost:8080/metrics
```

## Exposing Metrics over HTTP

```c
int bb_server_enable_metrics(bb_server_t *server, const char *path);
```

Registers `GET <path>` and serves the current metrics in Prometheus text exposition format, version `0.0.4`, with the content type:

`text/plain; version=0.0.4; charset=utf-8`

- `path` may be `NULL`, which means `"/metrics"`.
- Returns `0` on success and `-1` if `server` is `NULL`, `path` does not start with `/`, the route could not be registered, or metrics were already enabled.
- Call it once per server.

The metrics route is an ordinary route, so the usual router and middleware rules apply:

- The first route registered for a path wins. A route you added earlier on the same path shadows the metrics route.
- Pre- and post-middleware run around it, so authentication middleware protects it like any other route.
- If pre-middleware returns an error, the server closes the connection without sending a response. Prometheus sees a failed scrape rather than an HTTP error status.

Metrics can reveal traffic volume and error rates. If the server is reachable from untrusted networks, protect the endpoint with middleware, use a different path, or apply network-level restrictions. An internal listener is another option if the server supports the required listener configuration.

## Reading Metrics in Code

```c
int bb_server_get_metrics(
    const bb_server_t *server,
    bb_metrics_snapshot_t *out
);
```

Copies the current metrics into `*out`. Returns `0` on success and `-1` if `server` or `out` is `NULL`.

This works whether or not `bb_server_enable_metrics()` was called.

```c
bb_metrics_snapshot_t snap;

if (bb_server_get_metrics(server, &snap) == 0)
{
    printf("requests: %llu, active connections: %llu\n",
           (unsigned long long) snap.counters.requests_total,
           (unsigned long long) snap.active_connections);
}
```

**Threading:** Metrics are plain integers without locking. The runtime drives the server from a single event loop, so call `bb_server_get_metrics()` from the runtime thread (for example, inside a route handler or runtime task) or while the runtime is stopped. Reading metrics from another thread while the server is running is a data race.

If the runtime becomes multithreaded, the counters and snapshot mechanism will need appropriate synchronization.

### Snapshot Structure

Declared in `blue-bird/web/metrics.h`, which is included by `server.h`.

```c
typedef struct {
    bb_metrics_t counters;
    uint64_t active_connections;
    uint64_t active_ws_sessions;
} bb_metrics_snapshot_t;
```

| Field | Kind | Meaning |
|---|---|---|
| `active_connections` | Gauge | Open HTTP connections. Connections upgraded to WebSocket are not counted. |
| `active_ws_sessions` | Gauge | Open WebSocket sessions. |
| `counters` | Counters and histogram data | Cumulative connection, request, response-class, and latency statistics. |

The gauges are derived from the live connection and WebSocket lists when the snapshot is taken, so they cannot drift from the actual list state.

The counter structure is:

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

Counters accumulate from server creation onward and reset when a new server instance is created.

- `responses_by_class[i]`: index `1` is `1xx`, through index `5` for `5xx`. Index `0` represents other status codes.
- `latency_bucket[i]`: per-bucket request counts, **not cumulative**. The final bucket represents `+Inf`.
- `latency_count` and `latency_sum_ms`: the number of observed requests and the sum of their latencies in milliseconds.

### Latency Buckets

```c
uint32_t bb_metrics_latency_bound_ms(size_t i);
```

Returns the inclusive upper bound, in milliseconds, of latency bucket `i`. For `i >= BB_METRICS_LATENCY_BOUNDS`, it returns `UINT32_MAX`, representing the `+Inf` bucket.

| Bucket | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Upper bound | 1 ms | 5 ms | 10 ms | 25 ms | 50 ms | 100 ms | 250 ms | 1000 ms | +Inf |

Constants:

- `BB_METRICS_LATENCY_BOUNDS`: `8`
- `BB_METRICS_LATENCY_BUCKETS`: `9`
- `BB_METRICS_STATUS_CLASSES`: `6`

Latency is measured in whole milliseconds. Consequently, requests faster than 1 ms fall into the first bucket; the histogram cannot distinguish their individual durations.

## Prometheus Output

All metrics are per server process and begin at zero when the server starts.

| Metric | Type | Description |
|---|---|---|
| `bluebird_http_connections_active` | Gauge | Open HTTP connections, excluding connections upgraded to WebSocket. |
| `bluebird_websocket_sessions_active` | Gauge | Open WebSocket sessions. |
| `bluebird_connections_accepted_total` | Counter | TCP connections accepted since server start. |
| `bluebird_websocket_sessions_opened_total` | Counter | WebSocket sessions opened since server start. |
| `bluebird_http_requests_total{status_class="..."}` | Counter | HTTP requests answered, grouped by response status class: `1xx`, `2xx`, `3xx`, `4xx`, `5xx`, or `other`. |
| `bluebird_http_request_duration_seconds` | Histogram | Request handling duration, exported as `_bucket{le="..."}`, `_sum`, and `_count`. |

Every `status_class` series is emitted, including series with a value of zero. This means queries can observe the full set of status classes without waiting for each class to occur.

### Example Output

The following is illustrative output; actual values depend on server activity.

```text
# HELP bluebird_http_connections_active Open HTTP connections (connections upgraded to WebSocket are not counted).
# TYPE bluebird_http_connections_active gauge
bluebird_http_connections_active 1

# HELP bluebird_websocket_sessions_active Open WebSocket sessions.
# TYPE bluebird_websocket_sessions_active gauge
bluebird_websocket_sessions_active 0

# HELP bluebird_connections_accepted_total TCP connections accepted since server start.
# TYPE bluebird_connections_accepted_total counter
bluebird_connections_accepted_total 128630

# HELP bluebird_websocket_sessions_opened_total WebSocket sessions opened since server start.
# TYPE bluebird_websocket_sessions_opened_total counter
bluebird_websocket_sessions_opened_total 0

# HELP bluebird_http_requests_total HTTP requests answered, by response status class.
# TYPE bluebird_http_requests_total counter
bluebird_http_requests_total{status_class="1xx"} 0
bluebird_http_requests_total{status_class="2xx"} 128626
bluebird_http_requests_total{status_class="3xx"} 0
bluebird_http_requests_total{status_class="4xx"} 0
bluebird_http_requests_total{status_class="5xx"} 0
bluebird_http_requests_total{status_class="other"} 0

# HELP bluebird_http_request_duration_seconds Time from the first read of a request to its response being queued (excludes the socket write).
# TYPE bluebird_http_request_duration_seconds histogram
bluebird_http_request_duration_seconds_bucket{le="0.001"} 128617
bluebird_http_request_duration_seconds_bucket{le="0.005"} 128624
bluebird_http_request_duration_seconds_bucket{le="0.01"} 128625
bluebird_http_request_duration_seconds_bucket{le="0.025"} 128626
bluebird_http_request_duration_seconds_bucket{le="0.05"} 128626
bluebird_http_request_duration_seconds_bucket{le="0.1"} 128626
bluebird_http_request_duration_seconds_bucket{le="0.25"} 128626
bluebird_http_request_duration_seconds_bucket{le="1"} 128626
bluebird_http_request_duration_seconds_bucket{le="+Inf"} 128626
bluebird_http_request_duration_seconds_sum 10.642
bluebird_http_request_duration_seconds_count 128626
```

The example values are illustrative and are not a performance guarantee.

## Notes on What Is Measured

### Request Duration

Latency is measured from the first read of a request until its response has been built and queued for writing. This includes request parsing, routing, middleware, and handler execution performed within that interval.

It does **not** include:

- Time spent waiting for the connection to be accepted.
- Time spent waiting for request data to be read while the event loop is busy with other connections.
- Idle time before the client sends request data.
- Time spent writing the response to the socket.

The histogram therefore measures server-side request handling time, not the latency observed by a client. Under saturation, client-observed latency can be substantially higher than the reported request duration.

Durations are measured in whole milliseconds, so requests faster than 1 ms are not individually distinguishable. A quantile that falls within the first bucket has limited resolution and cannot establish the precise latency below that boundary.

### Counting Rules

- **Completed responses:** A request is counted when its response is queued. Requests rejected by middleware are counted if they receive a response, using the resulting status class.
- **WebSocket upgrades:** An upgrade is counted as one HTTP request with status `101` (`1xx`). Subsequent WebSocket frames are not HTTP requests and do not contribute to the HTTP request counters or latency histogram.
- **Unanswered requests:** Parse errors, handler or middleware errors that prevent a response from being queued, and clients that disconnect before a response is queued are not counted as answered requests. They may still contribute to the accepted-connection counter.
- **Metrics scrapes:** A scrape sees the state from just before its own response is completed. Its connection appears in the active HTTP connection gauge, but the scrape request is not yet included in the totals returned by that response.
- **Connection behavior:** The server closes the connection after each HTTP response and sends `Connection: close`. Each request therefore uses a new connection, so `bluebird_connections_accepted_total` grows with request volume under normal HTTP traffic.

A pre-middleware error that closes the connection without sending a response causes a failed scrape rather than an HTTP status response. This is distinct from a request that receives an HTTP error status.

### Histogram Semantics

The internal C histogram stores **non-cumulative** bucket counts. Prometheus output converts these to **cumulative** bucket counts, as required by the Prometheus histogram format.

For the Prometheus histogram:

- `_bucket{le="0.001"}` counts observations with a duration at or below 1 ms.
- Each successive finite bucket includes observations counted by the previous buckets.
- `_bucket{le="+Inf"}` equals `_count`.
- `_sum` is the sum of observed durations in seconds.
- `_count` is the total number of observed durations.

The exported seconds values are derived from the millisecond-based internal measurements.

## Prometheus Configuration

Add the following to `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: bluebird
    metrics_path: /metrics
    static_configs:
      - targets: ["localhost:8080"]
```

The example assumes Prometheus can reach the server at `localhost:8080`. Replace the target with the address reachable from the Prometheus process or container.

Because the server closes each HTTP connection after the response, each scrape opens a new connection. This is normal for Prometheus.

## Useful PromQL Queries

### Requests per second

```promql
sum(rate(bluebird_http_requests_total[1m]))
```

### Requests per second by status class

```promql
sum by (status_class) (
  rate(bluebird_http_requests_total[1m])
)
```

### Share of requests that are server errors

```promql
sum(rate(bluebird_http_requests_total{status_class="5xx"}[5m]))
/
sum(rate(bluebird_http_requests_total[5m]))
```

This ratio is undefined when there are no requests in the selected interval.

### 95th-percentile request duration

```promql
histogram_quantile(
  0.95,
  sum by (le) (
    rate(bluebird_http_request_duration_seconds_bucket[5m])
  )
)
```

### Mean request duration

```promql
rate(bluebird_http_request_duration_seconds_sum[5m])
/
rate(bluebird_http_request_duration_seconds_count[5m])
```

This ratio is undefined when no requests have been observed in the selected interval.

### Active connections and WebSocket sessions

```promql
bluebird_http_connections_active
bluebird_websocket_sessions_active
```

A gauge that keeps climbing while traffic is flat may indicate connections or WebSocket sessions that are not being closed as expected. Investigate the corresponding connection lifecycle before concluding that there is a leak.

Histogram quantiles are estimates derived from bucket boundaries. With the first bucket at 1 ms, a quantile falling inside that bucket has limited resolution below 1 ms.

## Rendering Metrics Yourself

The Prometheus renderer (`bb_metrics_render_prometheus()`) is an internal function used by the built-in route.

To publish metrics in another format or on a custom route, read a snapshot with `bb_server_get_metrics()` from a handler and format the fields yourself:

```c
bb_error_t stats_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;

    // `server` is the application's own bb_server_t pointer.
    bb_metrics_snapshot_t snap;

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

Ensure that the `server` pointer is available in the handler's scope and that the body-setting API handles the supplied buffer as expected.

## Load Testing and Verification

The load test in [`tests/load/`](../../tests/load/README.md) drives the server with `wrk` and cross-checks the results against the server's own metrics.

The checks cover:

- The HTTP `2xx` counter against the number of completed successful requests observed by `wrk`.
- The histogram count against the request counters.
- The active connection gauge returning to its expected idle value after the load test.

These checks help detect discrepancies between externally observed request completion and internally recorded metrics, as well as connection-lifecycle problems.

See [`tests/load/README.md`](../../tests/load/README.md) for the load-test setup and execution instructions.

## Possible Improvements

The following are ideas for future work, not features currently implemented by the metrics system.

1. **Measure latency from connection acceptance.** Add a second histogram measured from connection acceptance until the response is queued. This would capture more of the waiting time associated with a busy single-threaded event loop and complement the existing request-handling histogram.
2. **Count unanswered requests.** Add counters for parse errors, handler or middleware failures, and early client disconnects, potentially grouped by failure reason.
3. **Add per-route metrics.** Break down request counts and durations by route pattern, such as `/users/:id`, rather than the raw request path. Use bounded route labels to avoid excessive Prometheus time-series cardinality.
4. **Improve duration resolution.** Use a higher-resolution clock to distinguish requests below 1 ms and support more useful latency buckets.
5. **Make histogram buckets configurable.** Allow applications to select bucket boundaries appropriate for their workloads.
6. **Measure traffic volume.** Track bytes read and written.
7. **Add WebSocket metrics.** Track messages and frames in and out, along with close codes.
8. **Add process metrics.** Expose information such as open file descriptors, resident memory, and uptime.
9. **Support concurrent metric access.** If the runtime becomes multithreaded, introduce appropriate atomic counters or synchronization for consistent snapshots.

## Related Documentation

- [`overview.md`](overview.md)
- [`middleware.md`](middleware.md)
- [`routing.md`](routing.md)
- [`tests/load/README.md`](../../tests/load/README.md)