#ifndef BB_HTTP_PARSER_INCREMENTAL_H
#define BB_HTTP_PARSER_INCREMENTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* --------------------------------------------------------------------- */
/* Limits                                                                 */
/* --------------------------------------------------------------------- */

#define BB_HTTP_MAX_REQUEST_LINE  8192
#define BB_HTTP_MAX_HEADER_SIZE   32768
#define BB_HTTP_MAX_HEADER_COUNT  100
#define BB_HTTP_MAX_BODY_SIZE     (16 * 1024 * 1024)
#define BB_HTTP_MAX_CHUNK_SIZE    (16 * 1024 * 1024)
#define BB_HTTP_MAX_CHUNK_LINE    1024
#define BB_HTTP_MAX_TRAILER_SIZE  8192
#define BB_HTTP_MAX_TRAILER_COUNT 50

/* Includes the terminating NUL. */
#define BB_HTTP_METHOD_SIZE  32
#define BB_HTTP_TARGET_SIZE  (BB_HTTP_MAX_REQUEST_LINE)

/* --------------------------------------------------------------------- */
/* Status / state                                                        */
/* --------------------------------------------------------------------- */

typedef enum {
    BB_HTTP_PARSE_INCOMPLETE = 0, /* Need more bytes. */
    BB_HTTP_PARSE_COMPLETE,       /* One complete request/message parsed. */
    BB_HTTP_PARSE_ERROR           /* Malformed or policy-violating input. */
} bb_http_parse_status_t;

typedef enum {
    BB_HTTP_PARSE_REQUEST_LINE,

    BB_HTTP_PARSE_HEADERS,

    BB_HTTP_PARSE_BODY_FIXED,      /* Content-Length */

    BB_HTTP_PARSE_CHUNK_SIZE,      /* chunk-size line */
    BB_HTTP_PARSE_CHUNK_DATA,      /* chunk payload */
    BB_HTTP_PARSE_CHUNK_DATA_CRLF, /* CRLF after payload */
    BB_HTTP_PARSE_TRAILERS,        /* trailers after final 0 chunk */

    BB_HTTP_PARSE_STATE_COMPLETE,
    BB_HTTP_PARSE_STATE_ERROR
} bb_http_parse_state_t;

/* --------------------------------------------------------------------- */
/* Parsed request                                                        */
/* --------------------------------------------------------------------- */

typedef struct {
    char *name;
    char *value;
} bb_http_header_t;

typedef struct {
    char method[BB_HTTP_METHOD_SIZE];
    char target[BB_HTTP_TARGET_SIZE];

    int version_major;
    int version_minor;

    bb_http_header_t *headers;
    size_t header_count;

    bb_http_header_t *trailers;
    size_t trailer_count;

    uint8_t *body;
    size_t body_len;
} bb_http_request_t;

typedef struct {
    int version_major;
    int version_minor;
    int status_code;
    char reason[BB_HTTP_MAX_REQUEST_LINE];

    bb_http_header_t *headers;
    size_t header_count;

    bb_http_header_t *trailers;
    size_t trailer_count;

    uint8_t *body;
    size_t body_len;
} bb_http_response_t;

/* --------------------------------------------------------------------- */
/* Parser                                                                 */
/* --------------------------------------------------------------------- */

typedef struct bb_http_parser bb_http_parser_t;

bb_http_parser_t *bb_http_parser_create(void);
bb_http_parser_t *bb_http_parser_create_response(void);
void bb_http_parser_destroy(bb_http_parser_t *parser);

/* Resets the parser (and the request it has accumulated) so it can be
 * reused to parse a new message on the same connection. */
void bb_http_parser_reset(bb_http_parser_t *parser);

/*
 * Feeds `len` bytes of freshly received data into the parser. `data` may
 * hold any fragment of the wire stream -- a partial token, a partial
 * header, a partial chunk, etc. The parser retains all state needed to
 * resume across calls.
 *
 * Returns BB_HTTP_PARSE_INCOMPLETE if more bytes are required,
 * BB_HTTP_PARSE_COMPLETE once a full request has been parsed (retrieve it
 * with bb_http_parser_get_request()), or BB_HTTP_PARSE_ERROR if the input
 * is malformed or violates a configured limit. Once BB_HTTP_PARSE_ERROR is
 * returned, the parser must not be fed further data for this message; the
 * connection should be closed. Once BB_HTTP_PARSE_COMPLETE is returned,
 * the parser must be reset before parsing another message.
 */
bb_http_parse_status_t bb_http_parser_feed(
    bb_http_parser_t *parser,
    const uint8_t *data,
    size_t len);

/* Number of bytes consumed from the buffer passed to the most recent
 * bb_http_parser_feed() call. Any bytes beyond this (only possible once
 * the return value is BB_HTTP_PARSE_COMPLETE, e.g. with pipelined
 * requests) belong to the next message and were not consumed. */
size_t bb_http_parser_last_consumed(const bb_http_parser_t *parser);

/* Only valid to call after bb_http_parser_feed() has returned
 * BB_HTTP_PARSE_COMPLETE, and before the next reset. Ownership stays with
 * the parser; the pointer is invalidated by bb_http_parser_reset() or
 * bb_http_parser_destroy(). */
const bb_http_request_t *bb_http_parser_get_request(const bb_http_parser_t *parser);
const bb_http_response_t *bb_http_parser_get_response(const bb_http_parser_t *parser);

/* Short, human-readable description of the most recent error. Only
 * meaningful after bb_http_parser_feed() has returned
 * BB_HTTP_PARSE_ERROR. */
const char *bb_http_parser_error(const bb_http_parser_t *parser);

#ifdef __cplusplus
}
#endif

#endif /* BB_HTTP_PARSER_INCREMENTAL_H */
