#include "http/server_request.h"
#include "blue-bird/utils/encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int is_valid_decoded_path_char(unsigned char c)
{
    if (c < 0x20 || c == 0x7F)
        return 0;

    switch (c)
    {
        case ' ':
        case '"':
        case '<':
        case '>':
        case '\\':
        case '^':
        case '`':
        case '{':
        case '|':
        case '}':
            return 0;
        default:
            return 1;
    }
}

/*
 * Decode the request path exactly once into a separate buffer.
 *
 * Query splitting must happen before this function is called. Consequently
 * an encoded '?' (%3F) is path data and cannot create a query delimiter.
 * Likewise, encoded '&' and '=' remain path data rather than becoming query
 * syntax.
 *
 * Encoded '/' and '\\' are rejected deliberately. Decoding either one would
 * change path segment boundaries after routing, allowing two layers to see
 * different resources (for example /a%2Fb versus /a/b).
 */
static int decode_path(const char *src, size_t src_len,
                       char *dst, size_t dst_cap)
{
    size_t out = 0;

    if (!src || !dst || dst_cap == 0 || src_len == 0)
        return -1;

    for (size_t i = 0; i < src_len; i++)
    {
        unsigned char c = (unsigned char)src[i];

        if (c == '%')
        {
            if (i + 2 >= src_len)
                return -1;

            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);
            if (hi < 0 || lo < 0)
                return -1;

            c = (unsigned char)((hi << 4) | lo);
            i += 2;

            /* Never allow decoding to change path structure. */
            if (c == '/' || c == '\\')
                return -1;
        }

        if (!is_valid_decoded_path_char(c))
            return -1;

        if (out + 1 >= dst_cap)
            return -1;

        dst[out++] = (char)c;
    }

    if (out == 0 || dst[0] != '/')
        return -1;

    dst[out] = '\0';
    return 0;
}

/*
 * Reject dot-segments after decoding. This catches literal, encoded, and
 * mixed representations such as ../, %2e%2e/, .%2e/, and %2e./.
 *
 * We reject rather than normalize: silently changing /a/../b into /b can
 * make authorization and routing layers disagree about the resource that
 * was actually requested.
 */
static int has_dot_segment(const char *path)
{
    const char *segment = path;

    if (*segment == '/')
        segment++;

    while (1)
    {
        const char *slash = strchr(segment, '/');
        size_t len = slash ? (size_t)(slash - segment) : strlen(segment);

        if ((len == 1 && segment[0] == '.') ||
            (len == 2 && segment[0] == '.' && segment[1] == '.'))
            return 1;

        if (!slash)
            return 0;

        segment = slash + 1;
    }
}

void bb_server_request_init(bb_server_request_t *req)
{
    if (!req) return;
    req->msg = bb_message_create();
    req->param_count = 0;
    req->query_count = 0;
}

void bb_server_request_destroy(bb_server_request_t *req)
{
    if (!req) return;
    if (req->msg)
    {
        bb_message_destroy(req->msg);
    }
    req->param_count = 0;
    req->query_count = 0;
}

void bb_server_request_reset(bb_server_request_t *req)
{
    if (!req) return;
    if (req->msg)
    {
        bb_message_reset(req->msg);
    }
    req->param_count = 0;
    req->query_count = 0;
}

/*
 * Decode one URI/query component in place.
 *
 * The HTTP parser has already checked percent-encoding syntax, but this
 * function deliberately validates it again because it is a separate trust
 * boundary. Decoding happens exactly once and never creates new query
 * separators: the query is split into fields before either side is decoded.
 */
static int decode_query_component(char *dst, size_t dst_cap,
                                  const char *src, size_t src_len)
{
    size_t out = 0;

    if (dst_cap == 0)
        return -1;

    for (size_t i = 0; i < src_len; i++)
    {
        unsigned char c = (unsigned char)src[i];

        if (c == '%')
        {
            if (i + 2 >= src_len)
                return -1;

            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);
            if (hi < 0 || lo < 0)
                return -1;

            c = (unsigned char)((hi << 4) | lo);
            i += 2;
        }
        else if (c == '+')
        {
            c = ' ';
        }

        /* Decoded NUL/control bytes must never enter C strings. */
        if (c == '\0' || c < 0x20 || c == 0x7F)
            return -1;

        if (out + 1 >= dst_cap)
            return -1;

        dst[out++] = (char)c;
    }

    dst[out] = '\0';
    return 0;
}

/*
 * Parse the raw query component without strtok(): strtok silently collapses
 * repeated separators and makes it impossible to distinguish malformed empty
 * parameters. The raw query is split first; percent-decoding is performed
 * afterwards so %26 and %3D remain data rather than becoming delimiters.
 */
static int parse_query_params(bb_server_request_t *req,
                              const char *query, size_t query_len)
{
    if (query_len == 0)
        return 0;

    if (query[query_len - 1] == '&')
        return -1;

    size_t pair_start = 0;

    while (pair_start < query_len)
    {
        size_t pair_end = pair_start;
        while (pair_end < query_len && query[pair_end] != '&')
            pair_end++;

        if (pair_end == pair_start)
            return -1; /* empty parameter, e.g. && */

        if (pair_end == query_len && pair_end > pair_start &&
            query[pair_end - 1] == '&')
            return -1; /* trailing '&' */

        if (req->query_count >= MAX_QUERY_PARAMS)
            return -1;

        size_t eq = pair_start;
        while (eq < pair_end && query[eq] != '=')
            eq++;

        size_t key_len = eq - pair_start;
        size_t value_start = eq < pair_end ? eq + 1 : pair_end;
        size_t value_len = pair_end - value_start;

        if (key_len == 0 || key_len >= MAX_QUERY_PARAM_KEY)
            return -1;
        if (value_len >= MAX_QUERY_PARAM_VALUE)
            return -1;

        char key[MAX_QUERY_PARAM_KEY];
        char value[MAX_QUERY_PARAM_VALUE];

        if (decode_query_component(key, sizeof(key),
                                   query + pair_start, key_len) < 0)
            return -1;
        if (decode_query_component(value, sizeof(value),
                                   query + value_start, value_len) < 0)
            return -1;

        /* Decoding must not create a separator/assignment ambiguity in the
         * key. An encoded '=' is data in the key, but accepting it makes the
         * key representation unnecessarily ambiguous to callers. */
        if (strchr(key, '=') || strchr(key, '&'))
            return -1;

        if (bb_server_request_add_query_param(req, key, value) < 0)
            return -1;

        pair_start = pair_end;
        if (pair_start < query_len)
            pair_start++; /* skip '&' */
    }

    return 0;
}

int bb_server_request_from_parsed(const bb_http_request_t *parsed, bb_server_request_t *req)
{
    if (!parsed || !req) return -1;

    if (strlen(parsed->method) >= METHOD_SIZE ||
        strlen(parsed->target) >= PATH_SIZE)
        return -1;

    req->param_count = 0;
    req->query_count = 0;
    bb_message_reset(req->msg);

    size_t method_len = strlen(parsed->method);
    const char *target = parsed->target;
    const char *qmark = strchr(target, '?');
    size_t raw_path_len = qmark ? (size_t)(qmark - target) : strlen(target);
    size_t query_len = qmark ? strlen(qmark + 1) : 0;

    if (raw_path_len == 0 || raw_path_len >= PATH_SIZE)
        return -1;

    memcpy(req->method, parsed->method, method_len + 1);
    snprintf(req->version, sizeof(req->version), "HTTP/%d.%d",
             parsed->version_major, parsed->version_minor);

    /*
     * Canonicalize the path exactly once, after separating the raw query.
     * This makes encoded reserved characters deterministic: %3F becomes '?'
     * in the path, while it can never become a query delimiter here.
     */
    if (decode_path(target, raw_path_len, req->path, sizeof(req->path)) < 0)
        return -1;

    /* Reject dot-segments after decoding, including encoded/mixed forms. */
    if (has_dot_segment(req->path))
        return -1;

    char start_line[PATH_SIZE + METHOD_SIZE + VERSION_SIZE + 3];
    snprintf(start_line, sizeof(start_line), "%s %s %s",
             req->method, req->path, req->version);
    bb_message_set_start_line(req->msg, start_line);

    for (size_t i = 0; i < parsed->header_count; i++)
        bb_message_set_header(req->msg, parsed->headers[i].name, parsed->headers[i].value);

    bb_message_set_body_data(req->msg, parsed->body, parsed->body_len);

    const char *content_type = bb_message_get_header(req->msg, "Content-Type");
    if (content_type && strncmp(content_type, "application/x-www-form-urlencoded", 33) == 0 &&
        bb_message_get_body(req->msg))
    {
        bb_decode_percent((char *)bb_message_get_body(req->msg), 1);
    }

    if (qmark && parse_query_params(req, qmark + 1, query_len) < 0)
        return -1;

    return 0;
}

int bb_server_request_parse(const char *raw, bb_server_request_t *req)
{
    if (!raw || !req) return -1;

    bb_http_parser_t *parser = bb_http_parser_create();
    if (!parser) return -1;

    bb_http_parse_status_t status = bb_http_parser_feed(
        parser, (const unsigned char *)raw, strlen(raw));

    int result = -1;
    if (status == BB_HTTP_PARSE_COMPLETE)
        result = bb_server_request_from_parsed(bb_http_parser_get_request(parser), req);

    bb_http_parser_destroy(parser);
    return result;
}

int bb_server_request_add_param(bb_server_request_t *req, const char *key, const char *value)
{
    if (req->param_count >= MAX_PARAMS)
       return -1;
    
    _bb_param_t *qp = &req->params[req->param_count];

    strncpy(qp->name, key, sizeof(qp->name) - 1);
    qp->name[sizeof(qp->name) - 1] = '\0';

    strncpy(qp->value, value, sizeof(qp->value) - 1);
    qp->value[sizeof(qp->value) - 1] = '\0';

    req->param_count++;
    return 0;
}

const char *bb_server_request_get_param(bb_server_request_t *req, const char *name)
{
    for (int i = 0; i < req->param_count; i++)
    {
        if (strcmp(req->params[i].name, name) == 0)
        {
            return req->params[i].value;
        }
    }
    return NULL;
}

int bb_server_request_add_query_param(bb_server_request_t *req, const char *key, const char *value)
{
    if (!req || !key || !value)
        return -1;

    if (req->query_count >= MAX_QUERY_PARAMS)
        return -1;

    if (strlen(key) >= MAX_QUERY_PARAM_KEY ||
        strlen(value) >= MAX_QUERY_PARAM_VALUE)
        return -1;

    _bb_query_param_t *qp = &req->query[req->query_count];

    memcpy(qp->key, key, strlen(key) + 1);
    memcpy(qp->value, value, strlen(value) + 1);

    req->query_count++;
    return 0;
}

const char *bb_server_request_get_query_param(bb_server_request_t *req, const char *key)
{
    for (int i = 0; i < req->query_count; i++)
    {
        if (strcmp(req->query[i].key, key) == 0)
        {
            return req->query[i].value;
        }
    }
    return NULL;
}

int bb_server_request_set_method(bb_server_request_t *req, const char *method)
{
    if (!req || !method) {
        return -1;
    }

    size_t len = strlen(method);
    if (len >= METHOD_SIZE) {
        return -1;
    }

    memcpy(req->method, method, len + 1);
    return 0;
}

int bb_server_request_set_path(bb_server_request_t *req, const char *path)
{
    if (!req || !path) {
        return -1;
    }

    size_t len = strlen(path);
    if (len >= PATH_SIZE) {
        return -1;
    }

    memcpy(req->path, path, len + 1);
    return 0;
}
