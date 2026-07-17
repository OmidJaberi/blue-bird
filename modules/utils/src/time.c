#if defined(_WIN32)
/* no POSIX feature-test macros needed on Windows */
#elif defined(__APPLE__)
#define _DARWIN_C_SOURCE   /* exposes timegm() etc. even with _XOPEN_SOURCE set */
#define _XOPEN_SOURCE 700
#else
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "blue-bird/utils/time.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ---------- Current Time ---------- */

int64_t bb_time_now_sec(void)
{
    return (int64_t)time(NULL);
}

int64_t bb_time_now_ms(void)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* FILETIME is 100ns ticks since 1601-01-01; shift to Unix epoch (ms) */
    return (int64_t)((t / 10000ULL) - 11644473600000ULL);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
#endif
}

int64_t bb_time_monotonic_ms(void)
{
#if defined(_WIN32)
    /* Querying the frequency each call avoids a non-thread-safe lazy-init
     * race; QPF is cheap and its result never changes at runtime. */
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (int64_t)((counter.QuadPart * 1000LL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
#endif
}

/* ---------- Formatting ---------- */

int bb_time_format_rfc1123(int64_t ts, char *buf, size_t bufsize)
{
    struct tm gm;
    time_t t = (time_t)ts;
#if defined(_WIN32)
    if (gmtime_s(&gm, &t) != 0)
        return 1;
#else
    if (!gmtime_r(&t, &gm))
        return 1;
#endif
    return strftime(buf, bufsize,
                    "%a, %d %b %Y %H:%M:%S GMT",
                    &gm) ? 0 : 1;
}

int bb_time_format_iso8601(int64_t ts, char *buf, size_t bufsize)
{
    struct tm gm;
    time_t t = (time_t)ts;
#if defined(_WIN32)
    if (gmtime_s(&gm, &t) != 0)
        return 1;
#else
    if (!gmtime_r(&t, &gm))
        return 1;
#endif
    return strftime(buf, bufsize,
                    "%Y-%m-%dT%H:%M:%SZ",
                    &gm) ? 0 : 1;
}

#if defined(_WIN32)
/* strptime() isn't available on Windows, so RFC1123 parsing is done
 * manually with sscanf + a small month-name lookup table. */
static int bb_month_from_abbrev(const char *mon)
{
    static const char *names[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    for (int i = 0; i < 12; i++)
    {
        if (_stricmp(mon, names[i]) == 0)
            return i;
    }
    return -1;
}
#endif

int bb_time_parse_rfc1123(const char *str, int64_t *out_ts)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

#if defined(_WIN32)
    char wday[4] = {0};
    char mon[4] = {0};
    int day, year, hour, min, sec;

    /* Format: "Sun, 06 Nov 1994 08:49:37 GMT" */
    int n = sscanf(str, "%3s, %d %3s %d %d:%d:%d GMT",
                   wday, &day, mon, &year, &hour, &min, &sec);
    if (n != 7)
        return 1;

    int month = bb_month_from_abbrev(mon);
    if (month < 0)
        return 1;

    tm.tm_mday = day;
    tm.tm_mon  = month;
    tm.tm_year = year - 1900;
    tm.tm_hour = hour;
    tm.tm_min  = min;
    tm.tm_sec  = sec;

    time_t t = _mkgmtime(&tm); /* Windows equivalent of timegm() */
    if (t == (time_t)-1)
        return 1;

    *out_ts = (int64_t)t;
    return 0;
#else
    char *ret = strptime(str,
                         "%a, %d %b %Y %H:%M:%S GMT",
                         &tm);
    if (!ret)
        return 1;

    *out_ts = (int64_t)timegm(&tm);
    return 0;
#endif
}