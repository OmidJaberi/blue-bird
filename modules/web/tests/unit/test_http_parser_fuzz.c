/*
 * Deterministic differential / property tests for the incremental HTTP parser
 * (both the request parser and the response parser used by the client).
 *
 * The central property: the parser's result must not depend on how TCP happens
 * to slice the byte stream. For every input -- valid, mutated or random -- the
 * outcome of feeding it in one piece must equal the outcome of feeding it in
 * arbitrary fragments (down to one byte at a time): same status, same parsed
 * message, same number of bytes consumed.
 *
 *   BB_FUZZ_SEED=<n>    seed (default: fixed)
 *   BB_FUZZ_ITERS=<n>   mutated inputs per corpus entry (default: 400)
 *
 * Run under ASan/UBSan.
 */

#include "http/parser.h"

#include <blue-bird/error/assert.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* PRNG                                                                 */
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

static uint32_t rnd(uint32_t bound)
{
    return bound ? (uint32_t)(rnd64() % bound) : 0;
}

/* ------------------------------------------------------------------ */
/* Canonical form of a parse outcome                                    */
/* ------------------------------------------------------------------ */

typedef struct
{
    char *data;
    size_t len, cap;
} sbuf_t;

static void sb_add(sbuf_t *b, const void *p, size_t n)
{
    if (n == 0)
        return;

    if (b->len + n + 1 > b->cap)
    {
        b->cap = (b->len + n + 1) * 2;
        b->data = realloc(b->data, b->cap);
        BB_ASSERT(b->data != NULL);
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void sb_str(sbuf_t *b, const char *s)
{
    sb_add(b, s, strlen(s));
    sb_add(b, "\x1f", 1);
}

static void sb_num(sbuf_t *b, long long v)
{
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%lld\x1f", v);
    sb_add(b, tmp, (size_t)n);
}

static void sb_headers(sbuf_t *b, const bb_http_header_t *h, size_t count)
{
    sb_num(b, (long long)count);
    for (size_t i = 0; i < count; i++)
    {
        sb_str(b, h[i].name ? h[i].name : "(null)");
        sb_str(b, h[i].value ? h[i].value : "(null)");
    }
}

static void canonicalize(sbuf_t *b, bb_http_parser_t *p, int is_response)
{
    if (!is_response)
    {
        const bb_http_request_t *r = bb_http_parser_get_request(p);
        BB_ASSERT(r != NULL);
        sb_str(b, r->method);
        sb_str(b, r->target);
        sb_num(b, r->version_major);
        sb_num(b, r->version_minor);
        sb_headers(b, r->headers, r->header_count);
        sb_headers(b, r->trailers, r->trailer_count);
        sb_num(b, (long long)r->body_len);
        sb_add(b, r->body, r->body_len);
    }
    else
    {
        const bb_http_response_t *r = bb_http_parser_get_response(p);
        BB_ASSERT(r != NULL);
        sb_num(b, r->version_major);
        sb_num(b, r->version_minor);
        sb_num(b, r->status_code);
        sb_str(b, r->reason);
        sb_headers(b, r->headers, r->header_count);
        sb_headers(b, r->trailers, r->trailer_count);
        sb_num(b, (long long)r->body_len);
        sb_add(b, r->body, r->body_len);
    }
}

/* ------------------------------------------------------------------ */
/* Running one parse                                                    */
/* ------------------------------------------------------------------ */

typedef struct
{
    bb_http_parse_status_t status;
    size_t consumed;
    sbuf_t canon; /* only filled for COMPLETE */
} outcome_t;

static void outcome_free(outcome_t *o)
{
    free(o->canon.data);
    memset(o, 0, sizeof(*o));
}

/* chunk_max == 0: feed everything in one call. Otherwise random pieces of 1..chunk_max bytes. */
static void run_parse(const uint8_t *data, size_t len, uint32_t chunk_max, int is_response, outcome_t *out)
{
    bb_http_parser_t *p = is_response ? bb_http_parser_create_response() : bb_http_parser_create();
    BB_ASSERT(p != NULL);

    memset(out, 0, sizeof(*out));
    out->status = BB_HTTP_PARSE_INCOMPLETE;

    size_t pos = 0;
    do
    {
        size_t n = len - pos;
        if (chunk_max != 0)
        {
            size_t piece = 1 + rnd(chunk_max);
            if (n > piece)
                n = piece;
        }

        out->status = bb_http_parser_feed(p, data + pos, n);
        size_t used = bb_http_parser_last_consumed(p);

        BB_ASSERT(used <= n);
        if (out->status == BB_HTTP_PARSE_INCOMPLETE)
            BB_ASSERT(used == n); /* the parser may only leave bytes behind once the message is complete */

        out->consumed += used;
        pos += n;
    } while (out->status == BB_HTTP_PARSE_INCOMPLETE && pos < len);

    if (out->status == BB_HTTP_PARSE_COMPLETE)
        canonicalize(&out->canon, p, is_response);

    bb_http_parser_destroy(p);
}

/* ------------------------------------------------------------------ */
/* Failure reporting                                                    */
/* ------------------------------------------------------------------ */

static const char *g_property = "";
static uint64_t g_iter = 0;
static const char *g_corpus_name = "";

static void dump_escaped(const uint8_t *p, size_t n)
{
    size_t shown = n > 400 ? 400 : n;
    for (size_t i = 0; i < shown; i++)
    {
        if (p[i] == '\r')
            fputs("\\r", stderr);
        else if (p[i] == '\n')
            fputs("\\n", stderr);
        else if (p[i] >= 0x20 && p[i] < 0x7F && p[i] != '\\')
            fputc(p[i], stderr);
        else
            fprintf(stderr, "\\x%02x", p[i]);
    }
    if (shown < n)
        fprintf(stderr, "... (%zu bytes)", n);
    fputc('\n', stderr);
}

#define FUZZ_CHECK(cond, bytes, nbytes)                                                                  \
    do                                                                                                   \
    {                                                                                                    \
        if (!(cond))                                                                                     \
        {                                                                                                \
            fprintf(stderr, "\nFUZZ FAILURE: %s [%s] (seed=%" PRIu64 " iteration=%" PRIu64 ")\n  check: %s\n  input: ", \
                    g_property, g_corpus_name, g_seed, g_iter, #cond);                                   \
            dump_escaped((const uint8_t *)(bytes), (nbytes));                                            \
            BB_ASSERT(cond);                                                                             \
        }                                                                                                \
    } while (0)

/* ------------------------------------------------------------------ */
/* Corpus: valid messages with known framing                            */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char *name;
    const char *msg;
    size_t len;
} corpus_entry_t;

#define ENTRY(name, str) {name, str, sizeof(str) - 1}

static const corpus_entry_t request_corpus[] = {
    ENTRY("get-minimal", "GET / HTTP/1.1\r\nHost: a\r\n\r\n"),
    ENTRY("http10", "GET /old HTTP/1.0\r\n\r\n"),
    ENTRY("post-content-length",
          "POST /submit?x=1&y=%20z HTTP/1.1\r\nHost: a\r\nContent-Type: text/plain\r\n"
          "Content-Length: 11\r\n\r\nhello world"),
    ENTRY("chunked-ext-trailer",
          "POST /c HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n"
          "5;ext=1\r\nhello\r\nA\r\n,blue-bird\r\n0\r\nX-T: v\r\n\r\n"),
    ENTRY("chunked-empty", "POST /c HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"),
    ENTRY("many-headers",
          "GET /a/b%20c?q=1 HTTP/1.1\r\nHost: a\r\nAccept: */*\r\nCookie: a=b; c=d\r\n"
          "User-Agent: fuzz/1.0\r\nX-Padding: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n\r\n"),
};

static const corpus_entry_t response_corpus[] = {
    ENTRY("ok-content-length", "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nhello"),
    ENTRY("empty-body", "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"),
    ENTRY("switching-protocols",
          "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
          "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n"),
    ENTRY("chunked",
          "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n"),
};

/* ------------------------------------------------------------------ */
/* Property 1: valid messages                                           */
/* ------------------------------------------------------------------ */

static void property_valid_messages(const corpus_entry_t *corpus, size_t count, int is_response, uint64_t iters)
{
    g_property = "valid message: strict prefixes are incomplete, whole is complete, any chunking agrees";

    for (size_t c = 0; c < count; c++)
    {
        const uint8_t *msg = (const uint8_t *)corpus[c].msg;
        size_t n = corpus[c].len;
        g_corpus_name = corpus[c].name;

        outcome_t whole;
        run_parse(msg, n, 0, is_response, &whole);

        FUZZ_CHECK(whole.status == BB_HTTP_PARSE_COMPLETE, msg, n);
        FUZZ_CHECK(whole.consumed == n, msg, n);

        /* every strict prefix of a valid message is merely incomplete */
        for (size_t prefix = 0; prefix < n; prefix++)
        {
            outcome_t o;
            g_iter = prefix;
            run_parse(msg, prefix, 0, is_response, &o);
            FUZZ_CHECK(o.status == BB_HTTP_PARSE_INCOMPLETE, msg, prefix);
            outcome_free(&o);
        }

        /* byte-at-a-time and random fragmentation agree with the one-shot parse */
        for (uint64_t it = 0; it <= iters; it++)
        {
            outcome_t o;
            g_iter = it;
            uint32_t chunk_max = (it == 0) ? 1 : 1 + rnd(16);
            run_parse(msg, n, chunk_max, is_response, &o);

            FUZZ_CHECK(o.status == whole.status, msg, n);
            FUZZ_CHECK(o.consumed == whole.consumed, msg, n);
            FUZZ_CHECK(o.canon.len == whole.canon.len &&
                           memcmp(o.canon.data, whole.canon.data, whole.canon.len) == 0,
                       msg, n);
            outcome_free(&o);
        }

        outcome_free(&whole);
    }
}

/* ------------------------------------------------------------------ */
/* Property 2: pipelined messages stop exactly at the message boundary   */
/* ------------------------------------------------------------------ */

static void property_pipelining(const corpus_entry_t *corpus, size_t count, int is_response, uint64_t iters)
{
    g_property = "pipelined messages: the first completes at exactly its own length";

    for (uint64_t it = 0; it < iters; it++)
    {
        const corpus_entry_t *a = &corpus[rnd((uint32_t)count)];
        const corpus_entry_t *b = &corpus[rnd((uint32_t)count)];
        g_corpus_name = a->name;
        g_iter = it;

        size_t total = a->len + b->len;
        uint8_t *both = malloc(total);
        BB_ASSERT(both != NULL);
        memcpy(both, a->msg, a->len);
        memcpy(both + a->len, b->msg, b->len);

        outcome_t one_shot, fragmented;
        run_parse(both, total, 0, is_response, &one_shot);
        run_parse(both, total, 1 + rnd(32), is_response, &fragmented);

        FUZZ_CHECK(one_shot.status == BB_HTTP_PARSE_COMPLETE, both, total);
        FUZZ_CHECK(one_shot.consumed == a->len, both, total);
        FUZZ_CHECK(fragmented.status == BB_HTTP_PARSE_COMPLETE, both, total);
        FUZZ_CHECK(fragmented.consumed == a->len, both, total);
        FUZZ_CHECK(one_shot.canon.len == fragmented.canon.len &&
                       memcmp(one_shot.canon.data, fragmented.canon.data, one_shot.canon.len) == 0,
                   both, total);

        outcome_free(&one_shot);
        outcome_free(&fragmented);
        free(both);
    }
}

/* ------------------------------------------------------------------ */
/* Property 3: mutated and random input                                 */
/* ------------------------------------------------------------------ */

static const char *const splices[] = {
    "\r\n", "\r\n\r\n", "\n", "\r", "\t", " ", ":", ": ", "%00", "%0d%0a", "%2e%2e", "..", "//",
    "Content-Length: 5\r\n", "Content-Length: 99999999999999999999\r\n", "Transfer-Encoding: chunked\r\n",
    "Transfer-Encoding: identity\r\n", "Host: b\r\n", "0\r\n\r\n", "FFFFFFFFFFFFFFFFF\r\n", "HTTP/1.1", "\xff", "\x80",
};

static size_t mutate(uint8_t **buf, size_t len)
{
    uint32_t edits = 1 + rnd(4);

    while (edits--)
    {
        if (len == 0)
            break;

        switch (rnd(7))
        {
        case 0: /* flip a bit */
            (*buf)[rnd((uint32_t)len)] ^= (uint8_t)(1u << rnd(8));
            break;
        case 1: /* overwrite with an interesting byte */
        {
            static const uint8_t interesting[] = {0, '\r', '\n', ' ', ':', '%', ';', ',', '/', '?', 0x80, 0xFF};
            (*buf)[rnd((uint32_t)len)] = interesting[rnd((uint32_t)sizeof(interesting))];
            break;
        }
        case 2: /* delete a byte */
        {
            size_t at = rnd((uint32_t)len);
            memmove(*buf + at, *buf + at + 1, len - at - 1);
            len--;
            break;
        }
        case 3: /* insert a random byte */
        {
            size_t at = rnd((uint32_t)len + 1);
            *buf = realloc(*buf, len + 2);
            BB_ASSERT(*buf != NULL);
            memmove(*buf + at + 1, *buf + at, len - at);
            (*buf)[at] = (uint8_t)rnd64();
            len++;
            break;
        }
        case 4: /* splice in a token */
        {
            const char *tok = splices[rnd((uint32_t)(sizeof(splices) / sizeof(splices[0])))];
            size_t tl = strlen(tok);
            size_t at = rnd((uint32_t)len + 1);
            *buf = realloc(*buf, len + tl + 1);
            BB_ASSERT(*buf != NULL);
            memmove(*buf + at + tl, *buf + at, len - at);
            memcpy(*buf + at, tok, tl);
            len += tl;
            break;
        }
        case 5: /* duplicate a slice */
        {
            size_t from = rnd((uint32_t)len);
            size_t sl = 1 + rnd((uint32_t)(len - from > 24 ? 24 : len - from));
            size_t at = rnd((uint32_t)len + 1);
            uint8_t tmp[24];
            memcpy(tmp, *buf + from, sl);
            *buf = realloc(*buf, len + sl + 1);
            BB_ASSERT(*buf != NULL);
            memmove(*buf + at + sl, *buf + at, len - at);
            memcpy(*buf + at, tmp, sl);
            len += sl;
            break;
        }
        default: /* truncate */
            len = rnd((uint32_t)len + 1);
            break;
        }
    }

    return len;
}

static void check_differential(const uint8_t *in, size_t len, int is_response)
{
    outcome_t whole, byte_wise, random_chunks;

    run_parse(in, len, 0, is_response, &whole);
    run_parse(in, len, 1, is_response, &byte_wise);
    run_parse(in, len, 1 + rnd(64), is_response, &random_chunks);

    FUZZ_CHECK(byte_wise.status == whole.status, in, len);
    FUZZ_CHECK(random_chunks.status == whole.status, in, len);

    if (whole.status == BB_HTTP_PARSE_COMPLETE)
    {
        FUZZ_CHECK(byte_wise.consumed == whole.consumed, in, len);
        FUZZ_CHECK(random_chunks.consumed == whole.consumed, in, len);
        FUZZ_CHECK(byte_wise.canon.len == whole.canon.len &&
                       memcmp(byte_wise.canon.data, whole.canon.data, whole.canon.len) == 0,
                   in, len);
        FUZZ_CHECK(random_chunks.canon.len == whole.canon.len &&
                       memcmp(random_chunks.canon.data, whole.canon.data, whole.canon.len) == 0,
                   in, len);
    }

    outcome_free(&whole);
    outcome_free(&byte_wise);
    outcome_free(&random_chunks);
}

static void property_mutations(const corpus_entry_t *corpus, size_t count, int is_response, uint64_t iters)
{
    g_property = "mutated message: outcome is independent of fragmentation";

    for (size_t c = 0; c < count; c++)
    {
        g_corpus_name = corpus[c].name;

        for (g_iter = 0; g_iter < iters; g_iter++)
        {
            size_t len = corpus[c].len;
            uint8_t *buf = malloc(len + 1);
            BB_ASSERT(buf != NULL);
            memcpy(buf, corpus[c].msg, len);

            len = mutate(&buf, len);
            check_differential(buf, len, is_response);
            free(buf);
        }
    }
}

static void property_random_garbage(int is_response, uint64_t iters)
{
    g_property = "random bytes: no crash, outcome is independent of fragmentation";
    g_corpus_name = "random";

    for (g_iter = 0; g_iter < iters * 2; g_iter++)
    {
        size_t len = rnd(400);
        uint8_t *buf = malloc(len ? len : 1);
        BB_ASSERT(buf != NULL);

        /* bias toward HTTP-looking bytes so the parser gets past the first token */
        static const char alphabet[] = "GETPOSHTP/1.0 :\r\n%?=&;,-_aA0123456789\x80\xff";
        for (size_t i = 0; i < len; i++)
            buf[i] = rnd(4) ? (uint8_t)alphabet[rnd((uint32_t)(sizeof(alphabet) - 1))] : (uint8_t)rnd64();

        check_differential(buf, len, is_response);
        free(buf);
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

    uint64_t iters = iters_env ? strtoull(iters_env, NULL, 0) : 400;

    printf("Running HTTP parser fuzz tests (seed=%" PRIu64 ", iterations=%" PRIu64 ")...\n", g_seed, iters);

    const size_t nreq = sizeof(request_corpus) / sizeof(request_corpus[0]);
    const size_t nres = sizeof(response_corpus) / sizeof(response_corpus[0]);

    printf("\tTesting valid requests under any fragmentation...\n");
    property_valid_messages(request_corpus, nreq, 0, iters / 4 + 1);

    printf("\tTesting valid responses under any fragmentation...\n");
    property_valid_messages(response_corpus, nres, 1, iters / 4 + 1);

    printf("\tTesting pipelined requests and responses...\n");
    property_pipelining(request_corpus, nreq, 0, iters);
    property_pipelining(response_corpus, nres, 1, iters);

    printf("\tTesting mutated requests...\n");
    property_mutations(request_corpus, nreq, 0, iters);

    printf("\tTesting mutated responses...\n");
    property_mutations(response_corpus, nres, 1, iters);

    printf("\tTesting random garbage...\n");
    property_random_garbage(0, iters);
    property_random_garbage(1, iters);

    printf("All tests passed.\n");
    return 0;
}
