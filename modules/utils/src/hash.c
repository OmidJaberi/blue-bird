#include "blue-bird/utils/hash.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BB_SHA1_BLOCK_SIZE 64

static uint32_t _bb_sha1_rotl(uint32_t value, int bits)
{
    return (value << bits) | (value >> (32 - bits));
}

void bb_sha1(const void *data, size_t length, unsigned char digest[BB_SHA1_DIGEST_LENGTH])
{
    const uint8_t *message = data;

    uint64_t bit_length = (uint64_t)length * 8;

    size_t padded_length = length + 1;

    while ((padded_length % 64) != 56)
    {
        padded_length++;
    }

    padded_length += 8;

    uint8_t *buffer = calloc(1, padded_length);

    if (!buffer)
    {
        memset(digest, 0, BB_SHA1_DIGEST_LENGTH);
        return;
    }

    memcpy(buffer, message, length);

    buffer[length] = 0x80;

    for (int i = 0; i < 8; ++i)
    {
        buffer[padded_length - 1 - i] = (uint8_t)(bit_length >> (8 * i));
    }

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    for (size_t chunk = 0; chunk < padded_length; chunk += 64)
    {
        uint32_t w[80];

        for (int i = 0; i < 16; ++i)
        {
            size_t j = chunk + (i * 4);

            w[i] =
                ((uint32_t)buffer[j] << 24) |
                ((uint32_t)buffer[j + 1] << 16) |
                ((uint32_t)buffer[j + 2] << 8) |
                ((uint32_t)buffer[j + 3]);
        }

        for (int i = 16; i < 80; ++i)
        {
            w[i] =
                _bb_sha1_rotl(
                    w[i - 3] ^
                    w[i - 8] ^
                    w[i - 14] ^
                    w[i - 16],
                    1
                );
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (int i = 0; i < 80; ++i)
        {
            uint32_t f;
            uint32_t k;

            if (i < 20)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = _bb_sha1_rotl(a, 5) + f + e + k + w[i];

            e = d;
            d = c;
            c = _bb_sha1_rotl(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    free(buffer);

    digest[0]  = (h0 >> 24) & 0xFF;
    digest[1]  = (h0 >> 16) & 0xFF;
    digest[2]  = (h0 >> 8) & 0xFF;
    digest[3]  = h0 & 0xFF;

    digest[4]  = (h1 >> 24) & 0xFF;
    digest[5]  = (h1 >> 16) & 0xFF;
    digest[6]  = (h1 >> 8) & 0xFF;
    digest[7]  = h1 & 0xFF;

    digest[8]  = (h2 >> 24) & 0xFF;
    digest[9]  = (h2 >> 16) & 0xFF;
    digest[10] = (h2 >> 8) & 0xFF;
    digest[11] = h2 & 0xFF;

    digest[12] = (h3 >> 24) & 0xFF;
    digest[13] = (h3 >> 16) & 0xFF;
    digest[14] = (h3 >> 8) & 0xFF;
    digest[15] = h3 & 0xFF;

    digest[16] = (h4 >> 24) & 0xFF;
    digest[17] = (h4 >> 16) & 0xFF;
    digest[18] = (h4 >> 8) & 0xFF;
    digest[19] = h4 & 0xFF;
}
