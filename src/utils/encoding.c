#include "utils/encoding.h"

static int hexval(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

void decode_percent(char *s, int decode_plus)
{
    char *src = s;
    char *dst = s;

    while (*src) {
        if (decode_plus && *src == '+') {
            *dst++ = ' ';
            src++;
        }
        else if (*src == '%' &&
                 hexval(src[1]) >= 0 &&
                 hexval(src[2]) >= 0)
        {
            int hi = hexval(src[1]);
            int lo = hexval(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        }
        else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}