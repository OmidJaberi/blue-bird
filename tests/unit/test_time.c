#include "utils/time.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>


void test_now_sec(void)
{
    printf("Testing now seconds...\n");
    int64_t t1 = bb_time_now_sec();
    int64_t t2 = bb_time_now_sec();

    assert(t1 > 0);
    assert(t2 >= t1);
}

void test_now_ms(void)
{
    printf("Testing now milliseconds...\n");
    int64_t t1 = bb_time_now_ms();
    int64_t t2 = bb_time_now_ms();

    assert(t1 > 0);
    assert(t2 >= t1);
}

void test_monotonic_increases(void)
{
    printf("Testing monotonic increases...\n");
    int64_t t1 = bb_time_monotonic_ms();

    /* sleep 10ms */
#if defined(_WIN32)
    Sleep(10);
#else
    usleep(10000);
#endif

    int64_t t2 = bb_time_monotonic_ms();

    assert(t2 > t1);
}

void test_rfc1123_format(void)
{
    printf("Testing rfc1123 format...\n");
    char buf[64];

    /* Known timestamp: 06 Nov 1994 08:49:37 GMT */
    int64_t ts = 784111777;

    int rc = bb_time_format_rfc1123(ts, buf, sizeof(buf));
    assert(rc == 0);

    assert(strcmp(buf, "Sun, 06 Nov 1994 08:49:37 GMT") == 0);
}

void test_iso8601_format(void)
{
    printf("Testing iso8601 format...\n");
    char buf[64];

    int64_t ts = 784111777;

    int rc = bb_time_format_iso8601(ts, buf, sizeof(buf));
    assert(rc == 0);

    assert(strcmp(buf, "1994-11-06T08:49:37Z") == 0);
}

void test_rfc1123_buffer_too_small(void)
{
    printf("Testing rfc1123 buffer too small...\n");
    char buf[8];

    int rc = bb_time_format_rfc1123(784111777, buf, sizeof(buf));

    assert(rc != 0);
}

#ifndef _WIN32
void test_rfc1123_parse(void)
{
    printf("Testing rfc1123 parsing...\n");
    const char *str = "Sun, 06 Nov 1994 08:49:37 GMT";
    int64_t ts = 0;

    int rc = bb_time_parse_rfc1123(str, &ts);

    assert(rc == 0);
    assert(ts == 784111777);
}
#endif

int main(void)
{
    test_now_sec();
    test_now_ms();
    test_monotonic_increases();
    test_rfc1123_format();
    test_iso8601_format();
    test_rfc1123_buffer_too_small();

#ifndef _WIN32
    test_rfc1123_parse();
#endif

    printf("All time util tests passed.\n");
    return 0;
}