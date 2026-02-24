#ifndef BB_UTILS_TIME_H
#define BB_UTILS_TIME_H

#include <stdint.h>
#include <stddef.h>

/* ---------- Timestamps ---------- */

/* Current Unix time in seconds */
int64_t bb_time_now_sec(void);

/* Current Unix time in milliseconds */
int64_t bb_time_now_ms(void);

/* Monotonic clock in milliseconds (for measuring durations) */
int64_t bb_time_monotonic_ms(void);

/* ---------- Formatting ---------- */

/*
 * Format timestamp (seconds since epoch) into:
 * RFC 1123 format (HTTP compliant)
 * Example: "Sun, 06 Nov 1994 08:49:37 GMT"
 */
int bb_time_format_rfc1123(int64_t ts, char *buf, size_t bufsize);

/*
 * Format timestamp into ISO8601 (UTC)
 * Example: "2026-02-24T18:32:15Z"
 */
int bb_time_format_iso8601(int64_t ts, char *buf, size_t bufsize);

/* ---------- Parsing ---------- */

/* Parse RFC1123 string into epoch seconds */
int bb_time_parse_rfc1123(const char *str, int64_t *out_ts);

#endif
