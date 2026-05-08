#include "utils/time.h"

#include <time.h>
#include <stdio.h>
#include <string.h>

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
    static LARGE_INTEGER freq;
    static int initialized = 0;
    LARGE_INTEGER counter;

    if (!initialized)
    {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }

    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000LL) / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
#endif
}

// Formatting

int bb_time_format_rfc1123(int64_t ts, char *buf, size_t bufsize)
{
    struct tm gm;
    time_t t = (time_t)ts;

#if defined(_WIN32)
    gmtime_s(&gm, &t);
#else
    gmtime_r(&t, &gm);
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
    gmtime_s(&gm, &t);
#else
    gmtime_r(&t, &gm);
#endif

    return strftime(buf, bufsize,
                    "%Y-%m-%dT%H:%M:%SZ",
                    &gm) ? 0 : 1;
}

int bb_time_parse_rfc1123(const char *str, int64_t *out_ts)
{
    struct tm tm = {0};

    char *ret = strptime(str,
                         "%a, %d %b %Y %H:%M:%S GMT",
                         &tm);
    if (!ret)
        return 1;

#if defined(_WIN32)
    return 1; // strptime not standard on Windows
#else
    *out_ts = (int64_t)timegm(&tm);
    return 0;
#endif
}
