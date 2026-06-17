#include "blue-bird/utils/encoding.h"

#include <stdint.h>
#include <stdlib.h>

static int hexval(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

void bb_decode_percent(char *s, int decode_plus)
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

static const char BB_BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

char *bb_base64_encode(const void *data, size_t length)
{
    const unsigned char *src = data;

    size_t output_length = 4 * ((length + 2) / 3);

    char *out = malloc(output_length + 1);

    if (!out)
    {
        return NULL;
    }

    size_t i = 0;
    size_t j = 0;

    while (i < length)
    {
        uint32_t octet_a = i < length ? src[i++] : 0;

        uint32_t octet_b = i < length ? src[i++] : 0;

        uint32_t octet_c = i < length ? src[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = BB_BASE64_TABLE[(triple >> 18) & 0x3F];

        out[j++] = BB_BASE64_TABLE[(triple >> 12) & 0x3F];

        out[j++] = BB_BASE64_TABLE[(triple >> 6) & 0x3F];

        out[j++] = BB_BASE64_TABLE[triple & 0x3F];
    }

    static const int mod_table[] = {0, 2, 1};

    for (int k = 0; k < mod_table[length % 3]; ++k)
    {
        out[output_length - 1 - k] = '=';
    }

    out[output_length] = '\0';

    return out;
}
