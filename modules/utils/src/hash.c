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

static const uint32_t _bb_sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t _bb_sha256_rotr(uint32_t value, int bits)
{
    return (value >> bits) | (value << (32 - bits));
}

/* Processes exactly one 64-byte block, updating `state` in place. */
static void _bb_sha256_compress(uint32_t state[8], const unsigned char block[BB_SHA256_BLOCK_SIZE])
{
    uint32_t w[64];

    for (int i = 0; i < 16; ++i)
    {
        size_t j = (size_t)i * 4;

        w[i] =
            ((uint32_t)block[j] << 24) |
            ((uint32_t)block[j + 1] << 16) |
            ((uint32_t)block[j + 2] << 8) |
            ((uint32_t)block[j + 3]);
    }

    for (int i = 16; i < 64; ++i)
    {
        uint32_t s0 =
            _bb_sha256_rotr(w[i - 15], 7) ^
            _bb_sha256_rotr(w[i - 15], 18) ^
            (w[i - 15] >> 3);

        uint32_t s1 =
            _bb_sha256_rotr(w[i - 2], 17) ^
            _bb_sha256_rotr(w[i - 2], 19) ^
            (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i)
    {
        uint32_t s1 =
            _bb_sha256_rotr(e, 6) ^
            _bb_sha256_rotr(e, 11) ^
            _bb_sha256_rotr(e, 25);

        uint32_t ch = (e & f) ^ ((~e) & g);

        uint32_t temp1 = h + s1 + ch + _bb_sha256_k[i] + w[i];

        uint32_t s0 =
            _bb_sha256_rotr(a, 2) ^
            _bb_sha256_rotr(a, 13) ^
            _bb_sha256_rotr(a, 22);

        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);

        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void bb_sha256_init(bb_sha256_ctx *ctx)
{
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;

    ctx->bit_length = 0;
    ctx->buffer_len = 0;
}

void bb_sha256_update(bb_sha256_ctx *ctx, const void *data, size_t length)
{
    const uint8_t *bytes = data;

    ctx->bit_length += (uint64_t)length * 8;

    /* Top up a partial block left over from the previous call. */
    if (ctx->buffer_len > 0)
    {
        size_t needed = BB_SHA256_BLOCK_SIZE - ctx->buffer_len;
        size_t take = (length < needed) ? length : needed;

        memcpy(ctx->buffer + ctx->buffer_len, bytes, take);

        ctx->buffer_len += take;
        bytes += take;
        length -= take;

        if (ctx->buffer_len == BB_SHA256_BLOCK_SIZE)
        {
            _bb_sha256_compress(ctx->state, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }

    /* Compress full blocks straight out of the input, no copying. */
    while (length >= BB_SHA256_BLOCK_SIZE)
    {
        _bb_sha256_compress(ctx->state, bytes);
        bytes += BB_SHA256_BLOCK_SIZE;
        length -= BB_SHA256_BLOCK_SIZE;
    }

    /* Stash any leftover partial block for next time. */
    if (length > 0)
    {
        memcpy(ctx->buffer, bytes, length);
        ctx->buffer_len = length;
    }
}

void bb_sha256_final(bb_sha256_ctx *ctx, unsigned char digest[BB_SHA256_DIGEST_LENGTH])
{
    uint64_t bit_length = ctx->bit_length;

    unsigned char pad = 0x80;
    bb_sha256_update(ctx, &pad, 1);

    unsigned char zero = 0x00;

    /* Pad with zeros until exactly 8 bytes remain in the block for the
     * bit-length field. */
    while (ctx->buffer_len != BB_SHA256_BLOCK_SIZE - 8)
    {
        bb_sha256_update(ctx, &zero, 1);
    }

    unsigned char length_bytes[8];

    for (int i = 0; i < 8; ++i)
    {
        length_bytes[i] = (unsigned char)(bit_length >> (8 * (7 - i)));
    }

    /* Bypass bb_sha256_update() here: it would (harmlessly, but
     * pointlessly) fold this into ctx->bit_length again. */
    memcpy(ctx->buffer + ctx->buffer_len, length_bytes, 8);
    _bb_sha256_compress(ctx->state, ctx->buffer);

    for (int i = 0; i < 8; ++i)
    {
        digest[i * 4]     = (ctx->state[i] >> 24) & 0xFF;
        digest[i * 4 + 1] = (ctx->state[i] >> 16) & 0xFF;
        digest[i * 4 + 2] = (ctx->state[i] >> 8) & 0xFF;
        digest[i * 4 + 3] = ctx->state[i] & 0xFF;
    }
}

void bb_sha256(const void *data, size_t length, unsigned char digest[BB_SHA256_DIGEST_LENGTH])
{
    bb_sha256_ctx ctx;

    bb_sha256_init(&ctx);
    bb_sha256_update(&ctx, data, length);
    bb_sha256_final(&ctx, digest);
}

void bb_hmac_sha256_init(bb_hmac_sha256_ctx *ctx, const void *key, size_t key_length)
{
    unsigned char key_block[BB_SHA256_BLOCK_SIZE];

    memset(key_block, 0, sizeof(key_block));

    if (key_length > BB_SHA256_BLOCK_SIZE)
    {
        /* Long keys are hashed down to digest length first. */
        bb_sha256(key, key_length, key_block);
    }
    else if (key_length > 0)
    {
        memcpy(key_block, key, key_length);
    }

    unsigned char ipad[BB_SHA256_BLOCK_SIZE];
    unsigned char opad[BB_SHA256_BLOCK_SIZE];

    for (size_t i = 0; i < BB_SHA256_BLOCK_SIZE; ++i)
    {
        ipad[i] = key_block[i] ^ 0x36;
        opad[i] = key_block[i] ^ 0x5c;
    }

    bb_sha256_init(&ctx->inner);
    bb_sha256_update(&ctx->inner, ipad, sizeof(ipad));

    bb_sha256_init(&ctx->outer_primed);
    bb_sha256_update(&ctx->outer_primed, opad, sizeof(opad));

    memset(key_block, 0, sizeof(key_block));
    memset(ipad, 0, sizeof(ipad));
    memset(opad, 0, sizeof(opad));
}

void bb_hmac_sha256_update(bb_hmac_sha256_ctx *ctx, const void *data, size_t length)
{
    bb_sha256_update(&ctx->inner, data, length);
}

void bb_hmac_sha256_final(bb_hmac_sha256_ctx *ctx, unsigned char digest[BB_SHA256_DIGEST_LENGTH])
{
    unsigned char inner_digest[BB_SHA256_DIGEST_LENGTH];

    bb_sha256_final(&ctx->inner, inner_digest);

    /* Resume the primed (opad-only) outer state; this is why
     * outer_primed is a separate, untouched context. */
    bb_sha256_ctx outer = ctx->outer_primed;

    bb_sha256_update(&outer, inner_digest, sizeof(inner_digest));
    bb_sha256_final(&outer, digest);

    memset(inner_digest, 0, sizeof(inner_digest));
}

void bb_hmac_sha256(
    const void *key, size_t key_length,
    const void *data, size_t data_length,
    unsigned char digest[BB_SHA256_DIGEST_LENGTH])
{
    bb_hmac_sha256_ctx ctx;

    bb_hmac_sha256_init(&ctx, key, key_length);
    bb_hmac_sha256_update(&ctx, data, data_length);
    bb_hmac_sha256_final(&ctx, digest);

    memset(&ctx, 0, sizeof(ctx));
}
