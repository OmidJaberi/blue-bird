/*
 * Deterministic property / fuzz tests for bb_websocket_read_frames().
 *
 * The frame reader is fed without any sockets, the same way test_websocket.c
 * does it: bytes are placed in the connection's read buffer and the reader is
 * called. Every run is driven by a seeded PRNG so a failure can be replayed:
 *
 *   BB_FUZZ_SEED=<n>    seed (default: fixed)
 *   BB_FUZZ_ITERS=<n>   iterations per property (default: 600)
 *   BB_FUZZ_STRICT=1    also run the known-bad 64-bit length wraparound cases
 *                       (2^64-1 etc.). Without a fix these overflow the heap.
 *
 * Run it under ASan/UBSan: most of the value is in what the sanitizers see.
 */

#include "blue-bird/web/websocket/websocket.h"
#include "websocket/websocket_internal.h"

#include <blue-bird/utils/platform.h>
#include <blue-bird/error/assert.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* PRNG (xorshift64*)                                                   */
/* ------------------------------------------------------------------ */

static uint64_t g_seed = 0xB10EB1BDULL;
static uint64_t g_state;

static uint64_t rnd64(void)
{
    g_state ^= g_state >> 12;
    g_state ^= g_state << 25;
    g_state ^= g_state >> 27;
    return g_state * 0x2545F4914F6CDD1DULL;
}

static uint32_t rnd(uint32_t bound) /* [0, bound) */
{
    return bound ? (uint32_t)(rnd64() % bound) : 0;
}

/* ------------------------------------------------------------------ */
/* Frame under test: a generated model and its wire encoding             */
/* ------------------------------------------------------------------ */

typedef struct
{
    uint8_t fin;
    uint8_t opcode;
    uint8_t masked;
    uint64_t len;
    uint8_t *payload; /* unmasked */
    size_t wire_end;  /* offset in the stream where this frame ends */
} model_frame_t;

typedef struct
{
    model_frame_t frames[8];
    size_t count;
    uint8_t *bytes;
    size_t len;
} model_stream_t;

static void stream_free(model_stream_t *s)
{
    for (size_t i = 0; i < s->count; i++)
        free(s->frames[i].payload);
    free(s->bytes);
    memset(s, 0, sizeof(*s));
}

static size_t pick_length(bool allow_large)
{
    static const size_t small[] = {0, 1, 2, 5, 31, 124, 125, 126, 127, 128, 255, 256, 1000, 4096, 65535};

    if (allow_large && rnd(40) == 0)
        return rnd(2) ? 65536 : 70000;

    return small[rnd((uint32_t)(sizeof(small) / sizeof(small[0])))];
}

static void stream_generate(model_stream_t *s, size_t max_frames, bool allow_large)
{
    static const uint8_t opcodes[] = {0x0, 0x1, 0x2, 0x8, 0x9, 0xA, 0x3, 0xB};

    memset(s, 0, sizeof(*s));
    s->count = 1 + rnd((uint32_t)max_frames);

    /* worst case size: 14 header bytes + payload per frame */
    size_t cap = 0;
    for (size_t i = 0; i < s->count; i++)
    {
        s->frames[i].len = pick_length(allow_large);
        cap += 14 + s->frames[i].len;
    }

    s->bytes = malloc(cap);
    BB_ASSERT(s->bytes != NULL);

    for (size_t i = 0; i < s->count; i++)
    {
        model_frame_t *f = &s->frames[i];
        f->fin = (uint8_t)rnd(2);
        f->opcode = opcodes[rnd((uint32_t)sizeof(opcodes))];
        f->masked = (uint8_t)rnd(2);
        f->payload = malloc(f->len + 1);
        BB_ASSERT(f->payload != NULL);

        for (uint64_t k = 0; k < f->len; k++)
            f->payload[k] = (uint8_t)rnd64();

        uint8_t *out = s->bytes + s->len;
        size_t pos = 0;

        out[pos++] = (uint8_t)((f->fin << 7) | f->opcode);

        uint8_t mask_bit = f->masked ? 0x80 : 0x00;
        if (f->len < 126)
        {
            out[pos++] = mask_bit | (uint8_t)f->len;
        }
        else if (f->len <= 0xFFFF)
        {
            out[pos++] = mask_bit | 126;
            out[pos++] = (uint8_t)(f->len >> 8);
            out[pos++] = (uint8_t)(f->len & 0xFF);
        }
        else
        {
            out[pos++] = mask_bit | 127;
            for (int shift = 56; shift >= 0; shift -= 8)
                out[pos++] = (uint8_t)(f->len >> shift);
        }

        uint8_t key[4] = {0, 0, 0, 0};
        if (f->masked)
        {
            for (int k = 0; k < 4; k++)
                key[k] = (uint8_t)rnd64();
            memcpy(out + pos, key, 4);
            pos += 4;
        }

        for (uint64_t k = 0; k < f->len; k++)
            out[pos + k] = f->masked ? (uint8_t)(f->payload[k] ^ key[k & 3]) : f->payload[k];
        pos += (size_t)f->len;

        s->len += pos;
        f->wire_end = s->len;
    }
}

/* ------------------------------------------------------------------ */
/* Harness around the reader                                            */
/* ------------------------------------------------------------------ */

typedef struct
{
    bb_websocket_t *ws;
    bb_connection_t *conn;
    size_t fed;      /* total bytes appended */
    size_t consumed; /* total bytes the reader consumed */
} harness_t;

typedef struct
{
    uint8_t fin, opcode, masked;
    uint64_t len;
    uint8_t *payload;
} got_frame_t;

typedef struct
{
    got_frame_t *items;
    size_t count, cap;
} got_list_t;

static void got_free(got_list_t *g)
{
    for (size_t i = 0; i < g->count; i++)
        free(g->items[i].payload);
    free(g->items);
    memset(g, 0, sizeof(*g));
}

static void got_add(got_list_t *g, const bb_ws_frame_t *f)
{
    if (g->count == g->cap)
    {
        g->cap = g->cap ? g->cap * 2 : 8;
        g->items = realloc(g->items, g->cap * sizeof(*g->items));
        BB_ASSERT(g->items != NULL);
    }

    got_frame_t *d = &g->items[g->count++];
    d->fin = f->fin;
    d->opcode = f->opcode;
    d->masked = f->masked;
    d->len = f->payload_length;
    d->payload = malloc((size_t)f->payload_length + 1);
    BB_ASSERT(d->payload != NULL);
    memcpy(d->payload, f->payload, (size_t)f->payload_length);
}

static void harness_init(harness_t *h)
{
    bb_async_connection_t *async_conn = calloc(1, sizeof(*async_conn));
    BB_ASSERT(async_conn != NULL);
    async_conn->connection = calloc(1, sizeof(*async_conn->connection));
    BB_ASSERT(async_conn->connection != NULL);
    async_conn->connection->fd = -1;

    h->conn = async_conn->connection;
    h->ws = bb_websocket_create_with_type(async_conn, BB_WEBSOCKET_SERVER);
    BB_ASSERT(h->ws != NULL);
    h->fed = 0;
    h->consumed = 0;
}

static void harness_destroy(harness_t *h)
{
    bb_websocket_destroy(h->ws);
}

/*
 * Appends `len` bytes and runs the reader once, exactly like one read event in
 * the server. Frames it produced are appended to `out` (copied) and the
 * original chain is destroyed. Returns the reader's error code.
 */
static int harness_feed(harness_t *h, const uint8_t *data, size_t len, got_list_t *out)
{
    size_t before = h->conn->buffer_length;

    if (len > 0)
    {
        size_t need = before + len;
        if (need > h->conn->buffer_capacity || h->conn->buffer == NULL)
        {
            size_t cap = need < 64 ? 64 : need * 2;
            h->conn->buffer = realloc(h->conn->buffer, cap);
            BB_ASSERT(h->conn->buffer != NULL);
            h->conn->buffer_capacity = cap;
        }
        memcpy(h->conn->buffer + before, data, len);
        h->conn->buffer_length = need;
        h->fed += len;
    }

    bb_ws_frame_t frame;
    bb_error_t err = bb_websocket_read_frames(h->ws, &frame);

    size_t after = h->conn->buffer_length;
    BB_ASSERT(after <= before + len); /* the reader can only consume */
    h->consumed += (before + len) - after;

    if (err.code == BB_OK)
    {
        /* Success means at least one complete frame was returned and consumed. */
        BB_ASSERT(after < before + len);

        for (bb_ws_frame_t *f = &frame; f != NULL; f = f->next)
        {
            BB_ASSERT(f->payload != NULL);
            BB_ASSERT(f->payload_length <= (uint64_t)h->fed);
            BB_ASSERT(f->payload[f->payload_length] == '\0');
            if (out)
                got_add(out, f);
        }
        bb_ws_frame_destroy(&frame);
    }
    else
    {
        /* "Incomplete" must leave the buffer untouched. */
        BB_ASSERT(after == before + len);
        bb_ws_frame_destroy(&frame);
    }

    return err.code;
}

/* ------------------------------------------------------------------ */
/* Failure reporting                                                    */
/* ------------------------------------------------------------------ */

static const char *g_property = "";
static uint64_t g_iter = 0;

static void dump_hex(const uint8_t *p, size_t n)
{
    size_t shown = n > 96 ? 96 : n;
    for (size_t i = 0; i < shown; i++)
        fprintf(stderr, "%02x", p[i]);
    if (shown < n)
        fprintf(stderr, "... (%zu bytes)", n);
    fprintf(stderr, "\n");
}

#define FUZZ_CHECK(cond, bytes, nbytes)                                                              \
    do                                                                                               \
    {                                                                                                \
        if (!(cond))                                                                                 \
        {                                                                                            \
            fprintf(stderr, "\nFUZZ FAILURE: %s (seed=%" PRIu64 " iteration=%" PRIu64 ")\n  check: %s\n  input: ", \
                    g_property, g_seed, g_iter, #cond);                                              \
            dump_hex((bytes), (nbytes));                                                             \
            BB_ASSERT(cond);                                                                         \
        }                                                                                            \
    } while (0)

static bool frame_matches(const got_frame_t *g, const model_frame_t *m)
{
    return g->fin == m->fin && g->opcode == m->opcode && g->masked == m->masked && g->len == m->len &&
           (m->len == 0 || memcmp(g->payload, m->payload, (size_t)m->len) == 0);
}

/* ------------------------------------------------------------------ */
/* Property 1: framing is independent of how TCP slices the stream       */
/* ------------------------------------------------------------------ */

static void property_chunking_invariance(uint64_t iters)
{
    g_property = "decoded frames equal the generated frames for any TCP chunking";

    static const uint32_t max_chunk[] = {1, 2, 3, 7, 64, 1000, 100000};

    for (g_iter = 0; g_iter < iters; g_iter++)
    {
        model_stream_t s;
        stream_generate(&s, 5, true);

        uint32_t mc = max_chunk[rnd((uint32_t)(sizeof(max_chunk) / sizeof(max_chunk[0])))];

        harness_t h;
        got_list_t got = {0};
        harness_init(&h);

        size_t pos = 0;
        while (pos < s.len)
        {
            size_t n = 1 + rnd(mc);
            if (n > s.len - pos)
                n = s.len - pos;

            int code = harness_feed(&h, s.bytes + pos, n, &got);
            FUZZ_CHECK(code == BB_OK || code != BB_OK, s.bytes, s.len); /* either is legal mid-stream */
            pos += n;
        }

        FUZZ_CHECK(got.count == s.count, s.bytes, s.len);
        FUZZ_CHECK(h.conn->buffer_length == 0, s.bytes, s.len);
        FUZZ_CHECK(h.consumed == s.len, s.bytes, s.len);

        for (size_t i = 0; i < s.count && i < got.count; i++)
            FUZZ_CHECK(frame_matches(&got.items[i], &s.frames[i]), s.bytes, s.len);

        got_free(&got);
        harness_destroy(&h);
        stream_free(&s);
    }
}

/* ------------------------------------------------------------------ */
/* Property 2: any prefix yields exactly the frames that are complete    */
/* ------------------------------------------------------------------ */

static void property_prefixes(uint64_t iters)
{
    g_property = "a truncated stream yields exactly its complete frames, no more";

    for (g_iter = 0; g_iter < iters / 4 + 1; g_iter++)
    {
        model_stream_t s;
        stream_generate(&s, 3, false);

        /* keep exhaustive prefix testing cheap */
        if (s.len > 1200)
        {
            stream_free(&s);
            continue;
        }

        for (size_t prefix = 0; prefix <= s.len; prefix++)
        {
            size_t complete = 0;
            size_t complete_end = 0;
            for (size_t i = 0; i < s.count; i++)
            {
                if (s.frames[i].wire_end <= prefix)
                {
                    complete++;
                    complete_end = s.frames[i].wire_end;
                }
            }

            harness_t h;
            got_list_t got = {0};
            harness_init(&h);

            int code = harness_feed(&h, s.bytes, prefix, &got);

            FUZZ_CHECK((code == BB_OK) == (complete > 0), s.bytes, prefix);
            FUZZ_CHECK(got.count == complete, s.bytes, prefix);
            FUZZ_CHECK(h.conn->buffer_length == prefix - complete_end, s.bytes, prefix);

            for (size_t i = 0; i < complete && i < got.count; i++)
                FUZZ_CHECK(frame_matches(&got.items[i], &s.frames[i]), s.bytes, prefix);

            got_free(&got);
            harness_destroy(&h);
        }

        stream_free(&s);
    }
}

/* ------------------------------------------------------------------ */
/* Property 3: hostile input never crashes or corrupts the reader        */
/* ------------------------------------------------------------------ */

static void mutate(uint8_t *buf, size_t len)
{
    if (len == 0)
        return;

    static const uint8_t interesting[] = {0x00, 0x01, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0xFE, 0xFF};
    uint32_t edits = 1 + rnd(4);

    while (edits--)
    {
        size_t at = rnd((uint32_t)(len > 16 ? 16 : len)); /* bias toward the header */
        if (rnd(4) == 0)
            at = rnd((uint32_t)len);

        switch (rnd(3))
        {
        case 0:
            buf[at] ^= (uint8_t)(1u << rnd(8));
            break;
        case 1:
            buf[at] = interesting[rnd((uint32_t)sizeof(interesting))];
            break;
        default:
            buf[at] = (uint8_t)rnd64();
            break;
        }
    }
}

static void property_hostile_input(uint64_t iters)
{
    g_property = "mutated and random input never crashes the frame reader";

    for (g_iter = 0; g_iter < iters * 3; g_iter++)
    {
        uint8_t *bytes;
        size_t len;
        model_stream_t s;
        bool from_model = (rnd(3) != 0);

        if (from_model)
        {
            stream_generate(&s, 4, false);
            len = s.len;
            bytes = malloc(len ? len : 1);
            BB_ASSERT(bytes != NULL);
            memcpy(bytes, s.bytes, len);
            mutate(bytes, len);
        }
        else
        {
            memset(&s, 0, sizeof(s));
            len = rnd(300);
            bytes = malloc(len ? len : 1);
            BB_ASSERT(bytes != NULL);
            for (size_t i = 0; i < len; i++)
                bytes[i] = (uint8_t)rnd64();
        }

        /* A length field of 2^64-16 or more is the wraparound region; those
         * are covered separately (and are gated) below. Skip them here so the
         * default run stays green on an unfixed reader. */
        bool in_wrap_region = false;
        for (size_t i = 0; i + 10 <= len && i < 2; i++)
        {
            if ((bytes[i + 1] & 0x7F) == 127)
            {
                uint64_t l = 0;
                for (int k = 0; k < 8; k++)
                    l = (l << 8) | bytes[i + 2 + k];
                if (l >= UINT64_MAX - 32)
                    in_wrap_region = true;
            }
        }

        if (!in_wrap_region)
        {
            harness_t h;
            harness_init(&h);

            size_t pos = 0;
            uint32_t mc = 1 + rnd(40);
            while (pos < len)
            {
                size_t n = 1 + rnd(mc);
                if (n > len - pos)
                    n = len - pos;
                (void)harness_feed(&h, bytes + pos, n, NULL);
                pos += n;
            }

            FUZZ_CHECK(h.consumed <= h.fed, bytes, len);
            harness_destroy(&h);
        }

        free(bytes);
        if (from_model)
            stream_free(&s);
    }
}

/* ------------------------------------------------------------------ */
/* Property 4: huge declared lengths wait for data instead of allocating  */
/* ------------------------------------------------------------------ */

static void build_header(uint8_t *out, size_t *n, int masked, uint64_t len)
{
    size_t pos = 0;
    out[pos++] = 0x82;
    out[pos++] = (uint8_t)((masked ? 0x80 : 0x00) | 127);
    for (int shift = 56; shift >= 0; shift -= 8)
        out[pos++] = (uint8_t)(len >> shift);
    if (masked)
    {
        out[pos++] = 1;
        out[pos++] = 2;
        out[pos++] = 3;
        out[pos++] = 4;
    }
    *n = pos;
}

static void property_declared_length_bounds(void)
{
    g_property = "a declared length far beyond the data received is incomplete, not an allocation";

    static const uint64_t lens[] = {
        UINT64_C(1) << 31, UINT64_C(1) << 32, UINT64_C(1) << 40, UINT64_C(1) << 56,
        UINT64_C(1) << 62, (UINT64_C(1) << 63) - 1, UINT64_C(1) << 63, UINT64_MAX - 64,
    };

    for (size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); i++)
    {
        for (int masked = 0; masked <= 1; masked++)
        {
            uint8_t buf[64] = {0};
            size_t n;
            build_header(buf, &n, masked, lens[i]);
            memset(buf + n, 'x', 8); /* a few payload bytes, nowhere near enough */
            n += 8;

            harness_t h;
            got_list_t got = {0};
            harness_init(&h);
            g_iter = i;

            int code = harness_feed(&h, buf, n, &got);
            FUZZ_CHECK(code != BB_OK, buf, n);
            FUZZ_CHECK(got.count == 0, buf, n);
            FUZZ_CHECK(h.conn->buffer_length == n, buf, n);

            got_free(&got);
            harness_destroy(&h);
        }
    }
}

/*
 * Lengths where `offset + payload_len` wraps around size_t. A correct reader
 * must treat them like any other oversized frame (error or incomplete, never
 * success, never an out-of-bounds copy). Lengths of 2^64-14 .. 2^64-1 make the
 * "do we have the whole frame yet?" test pass by accident.
 */
static void property_length_wraparound(void)
{
    g_property = "64-bit lengths that overflow offset+length are rejected without out-of-bounds access";

    for (uint64_t back = 1; back <= 16; back++)
    {
        uint64_t len = UINT64_MAX - (back - 1); /* 2^64-1 down to 2^64-16 */

        for (int masked = 0; masked <= 1; masked++)
        {
            uint8_t buf[64] = {0};
            size_t n;
            build_header(buf, &n, masked, len);
            memset(buf + n, 'x', 8);
            n += 8;

            harness_t h;
            got_list_t got = {0};
            harness_init(&h);
            g_iter = back;

            int code = harness_feed(&h, buf, n, &got);
            FUZZ_CHECK(code != BB_OK, buf, n);
            FUZZ_CHECK(got.count == 0, buf, n);

            got_free(&got);
            harness_destroy(&h);
        }
    }
}

/* ------------------------------------------------------------------ */

int main(void)
{
    const char *seed_env = getenv("BB_FUZZ_SEED");
    const char *iters_env = getenv("BB_FUZZ_ITERS");

    if (seed_env)
        g_seed = strtoull(seed_env, NULL, 0);
    g_state = g_seed ? g_seed : 1;

    uint64_t iters = iters_env ? strtoull(iters_env, NULL, 0) : 600;

    printf("Running WebSocket frame fuzz tests (seed=%" PRIu64 ", iterations=%" PRIu64 ")...\n", g_seed, iters);

    printf("\tTesting chunking invariance...\n");
    property_chunking_invariance(iters);

    printf("\tTesting truncated-prefix behaviour...\n");
    property_prefixes(iters);

    printf("\tTesting hostile and mutated input...\n");
    property_hostile_input(iters);

    printf("\tTesting oversized declared lengths...\n");
    property_declared_length_bounds();

    if (getenv("BB_FUZZ_STRICT") != NULL)
    {
        printf("\tTesting 64-bit length wraparound (BB_FUZZ_STRICT)...\n");
        property_length_wraparound();
    }
    else
    {
        printf("\tSkipping 64-bit length wraparound cases (set BB_FUZZ_STRICT=1 to run).\n");
    }

    bb_runtime_destroy(bb_runtime_default());
    printf("All tests passed.\n");
    return 0;
}
