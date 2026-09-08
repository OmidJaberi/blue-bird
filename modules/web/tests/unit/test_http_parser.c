#include "http/parser.h"
#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* Helpers                                                               */
/* --------------------------------------------------------------------- */

static bb_http_parse_status_t feed_str(bb_http_parser_t *p, const char *s)
{
    return bb_http_parser_feed(p, (const uint8_t *)s, strlen(s));
}

/* Feeds a whole message one byte at a time; asserts INCOMPLETE for every
 * byte except the last, which must return `expect`. */
static void feed_bytewise(bb_http_parser_t *p, const char *s, bb_http_parse_status_t expect)
{
    size_t len = strlen(s);
    for (size_t i = 0; i < len; i++)
    {
        bb_http_parse_status_t st = bb_http_parser_feed(p, (const uint8_t *)s + i, 1);
        if (i + 1 < len)
        {
            BB_ASSERT(st == BB_HTTP_PARSE_INCOMPLETE);
        }
        else
        {
            BB_ASSERT(st == expect);
        }
    }
}

static const char *hdr(const bb_http_request_t *req, const char *name)
{
    for (size_t i = 0; i < req->header_count; i++)
        if (bb_strcasecmp(req->headers[i].name, name) == 0)
            return req->headers[i].value;
    return NULL;
}

static const char *trailer(const bb_http_request_t *req, const char *name)
{
    for (size_t i = 0; i < req->trailer_count; i++)
        if (bb_strcasecmp(req->trailers[i].name, name) == 0)
            return req->trailers[i].value;
    return NULL;
}

/* --------------------------------------------------------------------- */
/* Basic requests / state transitions                                    */
/* --------------------------------------------------------------------- */

void test_simple_request_no_body(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);

    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req != NULL);
    BB_ASSERT(strcmp(req->method, "GET") == 0);
    BB_ASSERT(strcmp(req->target, "/index.html") == 0);
    BB_ASSERT(req->version_major == 1 && req->version_minor == 1);
    BB_ASSERT(req->header_count == 1);
    BB_ASSERT(strcmp(hdr(req, "Host"), "example.com") == 0);
    BB_ASSERT(req->body_len == 0);

    bb_http_parser_destroy(p);
}

void test_request_with_fixed_body(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);

    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 5);
    BB_ASSERT(memcmp(req->body, "hello", 5) == 0);

    bb_http_parser_destroy(p);
}

void test_zero_length_content_length_body(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /submit HTTP/1.1\r\n"
        "Content-Length: 0\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 0);

    bb_http_parser_destroy(p);
}

/* --------------------------------------------------------------------- */
/* Arbitrary fragmentation                                                */
/* --------------------------------------------------------------------- */

void test_fragmented_across_every_token(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    BB_ASSERT(feed_str(p, "GET / HT") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "TP/1.1\r\nHost: ex") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "ample.com\r\nContent-Le") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "ngth: 5\r\n\r\nhe") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "llo") == BB_HTTP_PARSE_COMPLETE);

    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(strcmp(req->method, "GET") == 0);
    BB_ASSERT(strcmp(hdr(req, "Host"), "example.com") == 0);
    BB_ASSERT(memcmp(req->body, "hello", 5) == 0);

    bb_http_parser_destroy(p);
}

void test_byte_by_byte_simple_request(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    feed_bytewise(p,
        "GET /a HTTP/1.1\r\n"
        "Host: a\r\n"
        "\r\n",
        BB_HTTP_PARSE_COMPLETE);

    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(strcmp(req->target, "/a") == 0);

    bb_http_parser_destroy(p);
}

void test_byte_by_byte_chunked_request(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    feed_bytewise(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4\r\n"
        "Wiki\r\n"
        "5\r\n"
        "pedia\r\n"
        "0\r\n"
        "\r\n",
        BB_HTTP_PARSE_COMPLETE);

    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 9);
    BB_ASSERT(memcmp(req->body, "Wikipedia", 9) == 0);

    bb_http_parser_destroy(p);
}

/* --------------------------------------------------------------------- */
/* Chunked transfer encoding                                             */
/* --------------------------------------------------------------------- */

void test_chunked_body(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4\r\n"
        "Wiki\r\n"
        "5\r\n"
        "pedia\r\n"
        "0\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 9);
    BB_ASSERT(memcmp(req->body, "Wikipedia", 9) == 0);

    bb_http_parser_destroy(p);
}

void test_chunked_with_extension(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4;ext=1\r\n"
        "data\r\n"
        "0\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 4);
    BB_ASSERT(memcmp(req->body, "data", 4) == 0);

    bb_http_parser_destroy(p);
}

void test_chunked_with_trailers(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "3\r\n"
        "abc\r\n"
        "0\r\n"
        "X-Trailer: value\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 3);
    BB_ASSERT(req->trailer_count == 1);
    BB_ASSERT(strcmp(trailer(req, "X-Trailer"), "value") == 0);

    bb_http_parser_destroy(p);
}

void test_chunked_zero_length_body(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 0);

    bb_http_parser_destroy(p);
}

void test_chunked_bad_data_crlf(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "3\r\n"
        "abcXX\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_chunked_bare_lf_terminator(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "3\r\n"
        "abc\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_chunk_size_overflow(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "ffffffffffffffffff\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_chunk_size_exceeds_limit(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    /* Larger than BB_HTTP_MAX_CHUNK_SIZE (16 MiB), but well within
     * uint64_t range: 0x2000000 == 32 MiB. */
    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "2000000\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_chunk_trailer_content_length_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n"
        "Content-Length: 5\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_unsupported_transfer_encoding(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: gzip\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_transfer_encoding_not_final_chunked(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /up HTTP/1.1\r\n"
        "Transfer-Encoding: chunked, gzip\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

/* --------------------------------------------------------------------- */
/* Content-Length edge cases / smuggling protections                     */
/* --------------------------------------------------------------------- */

void test_duplicate_content_length_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /a HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_conflicting_content_length_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /a HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "Content-Length: 10\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_content_length_and_transfer_encoding_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /a HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n0\r\n\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_content_length_non_decimal_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /a HTTP/1.1\r\n"
        "Content-Length: 5a\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_content_length_empty_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /a HTTP/1.1\r\n"
        "Content-Length: \r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_content_length_overflow_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /a HTTP/1.1\r\n"
        "Content-Length: 99999999999999999999999999\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_content_length_exceeds_max_body_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "POST /a HTTP/1.1\r\n"
        "Content-Length: 99999999999\r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

void test_body_split_across_many_calls(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    BB_ASSERT(feed_str(p, "POST /a HTTP/1.1\r\nContent-Length: 10\r\n\r\n") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "ab") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "cd") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "ef") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "gh") == BB_HTTP_PARSE_INCOMPLETE);
    BB_ASSERT(feed_str(p, "ij") == BB_HTTP_PARSE_COMPLETE);

    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(req->body_len == 10);
    BB_ASSERT(memcmp(req->body, "abcdefghij", 10) == 0);

    bb_http_parser_destroy(p);
}

/* --------------------------------------------------------------------- */
/* Request line validation                                               */
/* --------------------------------------------------------------------- */

void test_malformed_request_line_missing_version(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, "GET /\r\n\r\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_malformed_request_line_empty_method(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, " / HTTP/1.1\r\n\r\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_malformed_request_line_bad_version(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, "GET / HTTP/11\r\n\r\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_malformed_request_line_bare_lf(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, "GET / HTTP/1.1\n\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_malformed_request_line_bare_cr(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, "GET / HTTP/1.1\rX") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_embedded_nul_in_request_line_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    uint8_t buf[] = "GET /\0x HTTP/1.1\r\n\r\n";
    BB_ASSERT(bb_http_parser_feed(p, buf, sizeof(buf) - 1) == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_request_line_exceeds_limit_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    size_t path_len = BB_HTTP_MAX_REQUEST_LINE + 10;
    char *line = malloc(path_len + 64);
    strcpy(line, "GET /");
    memset(line + 5, 'a', path_len);
    strcpy(line + 5 + path_len, " HTTP/1.1\r\n\r\n");

    BB_ASSERT(feed_str(p, line) == BB_HTTP_PARSE_ERROR);

    free(line);
    bb_http_parser_destroy(p);
}

void test_double_space_request_line_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, "GET  / HTTP/1.1\r\n\r\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

/* --------------------------------------------------------------------- */
/* Header validation                                                     */
/* --------------------------------------------------------------------- */

void test_header_missing_colon_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, "GET / HTTP/1.1\r\nHost example.com\r\n\r\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_header_invalid_name_char_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p, "GET / HTTP/1.1\r\nHo st: x\r\n\r\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_obsolete_folded_header_rejected(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    BB_ASSERT(feed_str(p,
        "GET / HTTP/1.1\r\n"
        "X-Foo: bar\r\n"
        " baz\r\n"
        "\r\n") == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_header_count_limit_exceeded(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    char buf[8192];
    size_t off = 0;
    off += (size_t)snprintf(buf + off, sizeof(buf) - off, "GET / HTTP/1.1\r\n");
    for (int i = 0; i < BB_HTTP_MAX_HEADER_COUNT + 1; i++)
        off += (size_t)snprintf(buf + off, sizeof(buf) - off, "X-H%d: v\r\n", i);
    off += (size_t)snprintf(buf + off, sizeof(buf) - off, "\r\n");

    BB_ASSERT(bb_http_parser_feed(p, (const uint8_t *)buf, off) == BB_HTTP_PARSE_ERROR);
    bb_http_parser_destroy(p);
}

void test_header_size_limit_exceeded(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    size_t value_len = BB_HTTP_MAX_HEADER_SIZE + 10;
    size_t buf_cap = value_len + 128;
    char *buf = malloc(buf_cap);
    size_t off = 0;
    off += (size_t)snprintf(buf + off, buf_cap - off, "GET / HTTP/1.1\r\nX-Big: ");
    memset(buf + off, 'a', value_len);
    off += value_len;
    off += (size_t)snprintf(buf + off, buf_cap - off, "\r\n\r\n");

    BB_ASSERT(bb_http_parser_feed(p, (const uint8_t *)buf, off) == BB_HTTP_PARSE_ERROR);

    free(buf);
    bb_http_parser_destroy(p);
}

void test_header_value_whitespace_trimmed(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    bb_http_parse_status_t st = feed_str(p,
        "GET / HTTP/1.1\r\n"
        "X-Foo:    bar   \r\n"
        "\r\n");

    BB_ASSERT(st == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req = bb_http_parser_get_request(p);
    BB_ASSERT(strcmp(hdr(req, "X-Foo"), "bar") == 0);

    bb_http_parser_destroy(p);
}

/* --------------------------------------------------------------------- */
/* Reset / reuse across messages                                         */
/* --------------------------------------------------------------------- */

void test_reset_allows_second_message(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    BB_ASSERT(feed_str(p, "GET /1 HTTP/1.1\r\n\r\n") == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req1 = bb_http_parser_get_request(p);
    BB_ASSERT(strcmp(req1->target, "/1") == 0);

    bb_http_parser_reset(p);

    BB_ASSERT(feed_str(p, "POST /2 HTTP/1.1\r\nContent-Length: 3\r\n\r\nabc") == BB_HTTP_PARSE_COMPLETE);
    const bb_http_request_t *req2 = bb_http_parser_get_request(p);
    BB_ASSERT(strcmp(req2->target, "/2") == 0);
    BB_ASSERT(req2->body_len == 3);

    bb_http_parser_destroy(p);
}

void test_error_is_terminal(void)
{
    bb_http_parser_t *p = bb_http_parser_create();

    BB_ASSERT(feed_str(p, "BAD REQUEST LINE HERE TOO MANY TOKENS\r\n") == BB_HTTP_PARSE_ERROR);
    /* Further feeding must keep reporting the error, not silently
     * resume parsing. */
    BB_ASSERT(feed_str(p, "GET / HTTP/1.1\r\n\r\n") == BB_HTTP_PARSE_ERROR);

    bb_http_parser_destroy(p);
}

/* --------------------------------------------------------------------- */

int main(void)
{
    printf("Running HTTP incremental parser tests...\n");

    test_simple_request_no_body();
    test_request_with_fixed_body();
    test_zero_length_content_length_body();

    test_fragmented_across_every_token();
    test_byte_by_byte_simple_request();
    test_byte_by_byte_chunked_request();

    test_chunked_body();
    test_chunked_with_extension();
    test_chunked_with_trailers();
    test_chunked_zero_length_body();
    test_chunked_bad_data_crlf();
    test_chunked_bare_lf_terminator();
    test_chunk_size_overflow();
    test_chunk_size_exceeds_limit();
    test_chunk_trailer_content_length_rejected();
    test_unsupported_transfer_encoding();
    test_transfer_encoding_not_final_chunked();

    test_duplicate_content_length_rejected();
    test_conflicting_content_length_rejected();
    test_content_length_and_transfer_encoding_rejected();
    test_content_length_non_decimal_rejected();
    test_content_length_empty_rejected();
    test_content_length_overflow_rejected();
    test_content_length_exceeds_max_body_rejected();
    test_body_split_across_many_calls();

    test_malformed_request_line_missing_version();
    test_malformed_request_line_empty_method();
    test_malformed_request_line_bad_version();
    test_malformed_request_line_bare_lf();
    test_malformed_request_line_bare_cr();
    test_embedded_nul_in_request_line_rejected();
    test_request_line_exceeds_limit_rejected();
    test_double_space_request_line_rejected();

    test_header_missing_colon_rejected();
    test_header_invalid_name_char_rejected();
    test_obsolete_folded_header_rejected();
    test_header_count_limit_exceeded();
    test_header_size_limit_exceeded();
    test_header_value_whitespace_trimmed();

    test_reset_allows_second_message();
    test_error_is_terminal();

    printf("All tests passed.\n");
    return 0;
}
