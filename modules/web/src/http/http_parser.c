#include "http/http_parser.h"

#include <blue-bird/utils/platform.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* --------------------------------------------------------------------- */
/* Parser state                                                          */
/* --------------------------------------------------------------------- */

struct bb_http_parser {
    bb_http_parse_state_t state;
    bool response_mode;

    /* Generic line accumulation buffer, reused for the request line,
     * each header line, each chunk-size line, and each trailer line. */
    uint8_t *line_buf;
    size_t line_len;
    size_t line_cap;
    size_t line_limit;   /* max characters allowed before CRLF for the
                           * line currently being read */
    bool line_saw_cr;

    bb_http_request_t request;
    bb_http_response_t response;

    bool has_content_length;
    uint64_t content_length;

    bool has_transfer_encoding;
    bool transfer_chunked;

    size_t header_bytes_total;
    size_t trailer_bytes_total;

    uint64_t body_cap;      /* allocated capacity of request.body */
    uint64_t body_written;  /* bytes written into request.body so far */

    uint64_t chunk_remaining; /* bytes remaining in the current chunk */

    size_t last_consumed;

    char error_msg[160];
};

/* --------------------------------------------------------------------- */
/* Small helpers                                                         */
/* --------------------------------------------------------------------- */

static void set_error(bb_http_parser_t *p, const char *msg)
{
    p->state = BB_HTTP_PARSE_STATE_ERROR;
    if (msg)
    {
        size_t n = strlen(msg);
        if (n >= sizeof(p->error_msg))
            n = sizeof(p->error_msg) - 1;
        memcpy(p->error_msg, msg, n);
        p->error_msg[n] = '\0';
    }
    else
    {
        p->error_msg[0] = '\0';
    }
}

static bool is_tchar(uint8_t c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        return true;

    switch (c)
    {
        case '!': case '#': case '$': case '%': case '&': case '\'':
        case '*': case '+': case '-': case '.': case '^': case '_':
        case '`': case '|': case '~':
            return true;
        default:
            return false;
    }
}

static void reset_line(bb_http_parser_t *p, size_t limit)
{
    p->line_len = 0;
    p->line_saw_cr = false;
    p->line_limit = limit;
}

static void free_headers(bb_http_header_t *headers, size_t count)
{
    if (!headers)
        return;
    for (size_t i = 0; i < count; i++)
    {
        free(headers[i].name);
        free(headers[i].value);
    }
    free(headers);
}

static void free_request(bb_http_request_t *req)
{
    free_headers(req->headers, req->header_count);
    free_headers(req->trailers, req->trailer_count);
    free(req->body);
    memset(req, 0, sizeof(*req));
}

static void free_response(bb_http_response_t *res)
{
    free_headers(res->headers, res->header_count);
    free_headers(res->trailers, res->trailer_count);
    free(res->body);
    memset(res, 0, sizeof(*res));
}

static bb_http_header_t **active_headers(bb_http_parser_t *p, size_t **count)
{
    if (p->response_mode)
    {
        *count = &p->response.header_count;
        return &p->response.headers;
    }
    *count = &p->request.header_count;
    return &p->request.headers;
}

static bb_http_header_t **active_trailers(bb_http_parser_t *p, size_t **count)
{
    if (p->response_mode)
    {
        *count = &p->response.trailer_count;
        return &p->response.trailers;
    }
    *count = &p->request.trailer_count;
    return &p->request.trailers;
}

static uint8_t **active_body(bb_http_parser_t *p)
{
    return p->response_mode ? &p->response.body : &p->request.body;
}

static size_t *active_body_len(bb_http_parser_t *p)
{
    return p->response_mode ? &p->response.body_len : &p->request.body_len;
}

/* --------------------------------------------------------------------- */
/* Lifecycle                                                             */
/* --------------------------------------------------------------------- */

bb_http_parser_t *bb_http_parser_create(void)
{
    bb_http_parser_t *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;

    p->state = BB_HTTP_PARSE_REQUEST_LINE;
    p->response_mode = false;
    reset_line(p, BB_HTTP_MAX_REQUEST_LINE);
    return p;
}

bb_http_parser_t *bb_http_parser_create_response(void)
{
    bb_http_parser_t *p = bb_http_parser_create();
    if (p)
        p->response_mode = true;
    return p;
}

void bb_http_parser_destroy(bb_http_parser_t *p)
{
    if (!p)
        return;

    free(p->line_buf);
    free_request(&p->request);
    free_response(&p->response);
    free(p);
}

void bb_http_parser_reset(bb_http_parser_t *p)
{
    if (!p)
        return;

    free_request(&p->request);
    free_response(&p->response);

    p->state = BB_HTTP_PARSE_REQUEST_LINE;
    reset_line(p, BB_HTTP_MAX_REQUEST_LINE);

    p->has_content_length = false;
    p->content_length = 0;

    p->has_transfer_encoding = false;
    p->transfer_chunked = false;

    p->header_bytes_total = 0;
    p->trailer_bytes_total = 0;

    p->body_cap = 0;
    p->body_written = 0;

    p->chunk_remaining = 0;

    p->last_consumed = 0;

    p->error_msg[0] = '\0';
}

size_t bb_http_parser_last_consumed(const bb_http_parser_t *p)
{
    return p ? p->last_consumed : 0;
}

const bb_http_request_t *bb_http_parser_get_request(const bb_http_parser_t *p)
{
    if (!p || p->state != BB_HTTP_PARSE_STATE_COMPLETE)
        return NULL;
    return &p->request;
}

const bb_http_response_t *bb_http_parser_get_response(const bb_http_parser_t *p)
{
    if (!p || !p->response_mode || p->state != BB_HTTP_PARSE_STATE_COMPLETE)
        return NULL;
    return &p->response;
}

const char *bb_http_parser_error(const bb_http_parser_t *p)
{
    return p ? p->error_msg : "";
}

/* --------------------------------------------------------------------- */
/* Line reading                                                          */
/* --------------------------------------------------------------------- */

/* Consumes bytes from 'data'/'len', appending to the parser's line buffer,
 * until a full CRLF-terminated line is found or input runs out.
 *
 * Returns  1 if a full line was consumed (excluding the CRLF); the line
 *            content is p->line_buf[0 .. p->line_len).
 *          0 if *len bytes were consumed and more data is required.
 *         -1 on a framing violation (bare CR, bare LF, embedded NUL) or
 *            if the line would exceed p->line_limit.
 */
static int consume_line(bb_http_parser_t *p, const uint8_t **data, size_t *len)
{
    while (*len > 0)
    {
        uint8_t c = **data;

        if (p->line_saw_cr)
        {
            (*data)++;
            (*len)--;
            p->line_saw_cr = false;

            if (c == '\n')
                return 1;

            set_error(p, "bare CR in line");
            return -1;
        }

        if (c == '\r')
        {
            p->line_saw_cr = true;
            (*data)++;
            (*len)--;
            continue;
        }

        if (c == '\n')
        {
            set_error(p, "bare LF in line");
            return -1;
        }

        if (c == '\0')
        {
            set_error(p, "embedded NUL");
            return -1;
        }

        if (p->line_len >= p->line_limit)
        {
            set_error(p, "line exceeds configured limit");
            return -1;
        }

        if (p->line_len + 1 > p->line_cap)
        {
            size_t new_cap = p->line_cap ? p->line_cap * 2 : 256;
            if (new_cap > p->line_limit + 1)
                new_cap = p->line_limit + 1;

            uint8_t *tmp = realloc(p->line_buf, new_cap);
            if (!tmp)
            {
                set_error(p, "out of memory");
                return -1;
            }
            p->line_buf = tmp;
            p->line_cap = new_cap;
        }

        p->line_buf[p->line_len++] = c;
        (*data)++;
        (*len)--;
    }

    return 0;
}

/* --------------------------------------------------------------------- */
/* Request line                                                          */
/* --------------------------------------------------------------------- */

static int parse_request_line(bb_http_parser_t *p)
{
    const uint8_t *buf = p->line_buf;
    size_t len = p->line_len;

    if (len == 0)
    {
        set_error(p, "empty request line");
        return -1;
    }

    /* method */
    size_t i = 0;
    while (i < len && buf[i] != ' ')
        i++;

    if (i == 0 || i == len)
    {
        set_error(p, "malformed request line: missing method or target");
        return -1;
    }

    size_t method_len = i;
    for (size_t k = 0; k < method_len; k++)
    {
        if (!is_tchar(buf[k]))
        {
            set_error(p, "invalid character in method");
            return -1;
        }
    }

    if (method_len >= sizeof(p->request.method))
    {
        set_error(p, "method too long");
        return -1;
    }
    memcpy(p->request.method, buf, method_len);
    p->request.method[method_len] = '\0';

    size_t rest_start = i + 1;

    /* version: last token, after the last space */
    size_t j = len;
    while (j > rest_start && buf[j - 1] != ' ')
        j--;

    if (j == rest_start)
    {
        set_error(p, "malformed request line: missing version");
        return -1;
    }

    size_t target_end = j - 1;      /* position of the last space */
    size_t version_start = j;

    if (target_end <= rest_start)
    {
        set_error(p, "malformed request line: empty target");
        return -1;
    }

    for (size_t k = rest_start; k < target_end; k++)
    {
        uint8_t c = buf[k];
        if (c == ' ' || c < 0x20 || c == 0x7F)
        {
            set_error(p, "invalid character in request-target");
            return -1;
        }
    }

    size_t target_len = target_end - rest_start;
    if (target_len >= sizeof(p->request.target))
    {
        set_error(p, "request-target too long");
        return -1;
    }
    memcpy(p->request.target, buf + rest_start, target_len);
    p->request.target[target_len] = '\0';

    size_t version_len = len - version_start;
    const uint8_t *v = buf + version_start;

    if (version_len != 8 || memcmp(v, "HTTP/", 5) != 0 ||
        !isdigit(v[5]) || v[6] != '.' || !isdigit(v[7]))
    {
        set_error(p, "invalid HTTP version");
        return -1;
    }

    p->request.version_major = v[5] - '0';
    p->request.version_minor = v[7] - '0';

    return 0;
}

static int parse_response_line(bb_http_parser_t *p)
{
    const uint8_t *buf = p->line_buf;
    size_t len = p->line_len;

    if (len < 12 || memcmp(buf, "HTTP/", 5) != 0)
    {
        set_error(p, "malformed response line");
        return -1;
    }

    if (!isdigit(buf[5]) || buf[6] != '.' || !isdigit(buf[7]) || buf[8] != ' ')
    {
        set_error(p, "invalid HTTP version");
        return -1;
    }

    size_t i = 9;
    if (i + 3 > len || !isdigit(buf[i]) || !isdigit(buf[i + 1]) || !isdigit(buf[i + 2]))
    {
        set_error(p, "invalid response status code");
        return -1;
    }

    p->response.version_major = buf[5] - '0';
    p->response.version_minor = buf[7] - '0';
    p->response.status_code = (buf[i] - '0') * 100 + (buf[i + 1] - '0') * 10 + (buf[i + 2] - '0');

    if (i + 3 < len && buf[i + 3] != ' ')
    {
        set_error(p, "malformed response reason phrase");
        return -1;
    }

    i += 3;
    if (i < len)
        i++;

    size_t reason_len = len - i;
    if (reason_len >= sizeof(p->response.reason))
    {
        set_error(p, "reason phrase too long");
        return -1;
    }

    for (size_t k = i; k < len; k++)
    {
        if ((buf[k] < 0x20 && buf[k] != '\t') || buf[k] == 0x7f)
        {
            set_error(p, "invalid character in reason phrase");
            return -1;
        }
    }

    memcpy(p->response.reason, buf + i, reason_len);
    p->response.reason[reason_len] = '\0';
    return 0;
}

/* --------------------------------------------------------------------- */
/* Header / trailer line                                                 */
/* --------------------------------------------------------------------- */

/* Parses p->line_buf as "name: value" and appends it to 'headers'/'count',
 * enforcing count/size limits. On success, 'out_name'/'out_value' point at
 * the newly stored strings (owned by the header array, do not free). */
static int parse_field_line(
    bb_http_parser_t *p,
    bb_http_header_t **headers,
    size_t *count,
    size_t max_count,
    size_t *bytes_total,
    size_t max_bytes,
    const char **out_name,
    const char **out_value)
{
    const uint8_t *buf = p->line_buf;
    size_t len = p->line_len;

    if (len > 0 && (buf[0] == ' ' || buf[0] == '\t'))
    {
        set_error(p, "obsolete line folding is not supported");
        return -1;
    }

    size_t i = 0;
    while (i < len && buf[i] != ':')
        i++;

    if (i == 0 || i == len)
    {
        set_error(p, "malformed field line: missing ':'");
        return -1;
    }

    size_t name_len = i;
    for (size_t k = 0; k < name_len; k++)
    {
        if (!is_tchar(buf[k]))
        {
            set_error(p, "invalid character in field name");
            return -1;
        }
    }

    size_t val_start = i + 1;
    size_t val_end = len;

    while (val_start < val_end && (buf[val_start] == ' ' || buf[val_start] == '\t'))
        val_start++;
    while (val_end > val_start && (buf[val_end - 1] == ' ' || buf[val_end - 1] == '\t'))
        val_end--;

    for (size_t k = val_start; k < val_end; k++)
    {
        uint8_t c = buf[k];
        if ((c < 0x20 && c != '\t') || c == 0x7F)
        {
            set_error(p, "invalid character in field value");
            return -1;
        }
    }

    if (*count >= max_count)
    {
        set_error(p, "too many header/trailer fields");
        return -1;
    }

    size_t line_bytes = len + 2; /* + CRLF */
    if (*bytes_total + line_bytes > max_bytes)
    {
        set_error(p, "header/trailer section too large");
        return -1;
    }

    size_t value_len = val_end - val_start;

    char *name = malloc(name_len + 1);
    char *value = malloc(value_len + 1);
    if (!name || !value)
    {
        free(name);
        free(value);
        set_error(p, "out of memory");
        return -1;
    }
    memcpy(name, buf, name_len);
    name[name_len] = '\0';
    memcpy(value, buf + val_start, value_len);
    value[value_len] = '\0';

    void *tmp = realloc(*headers, (*count + 1) * sizeof(**headers));
    if (!tmp)
    {
        free(name);
        free(value);
        set_error(p, "out of memory");
        return -1;
    }
    *headers = tmp;
    (*headers)[*count].name = name;
    (*headers)[*count].value = value;
    (*count)++;
    *bytes_total += line_bytes;

    *out_name = name;
    *out_value = value;
    return 0;
}

/* --------------------------------------------------------------------- */
/* Content-Length / Transfer-Encoding                                    */
/* --------------------------------------------------------------------- */

static int apply_content_length(bb_http_parser_t *p, const char *value)
{
    if (p->has_content_length)
    {
        set_error(p, "duplicate Content-Length header");
        return -1;
    }

    size_t len = strlen(value);
    if (len == 0)
    {
        set_error(p, "empty Content-Length value");
        return -1;
    }

    uint64_t val = 0;
    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)value[i];
        if (c < '0' || c > '9')
        {
            set_error(p, "non-decimal character in Content-Length");
            return -1;
        }
        unsigned digit = (unsigned)(c - '0');

        if (val > (UINT64_MAX - digit) / 10)
        {
            set_error(p, "Content-Length overflow");
            return -1;
        }
        val = val * 10 + digit;
    }

    if (val > BB_HTTP_MAX_BODY_SIZE)
    {
        set_error(p, "Content-Length exceeds configured body limit");
        return -1;
    }

    p->has_content_length = true;
    p->content_length = val;
    return 0;
}

static int apply_transfer_encoding(bb_http_parser_t *p, const char *value)
{
    /* Comma-separated list of codings; we only support "chunked", and
     * reject anything else outright rather than silently ignoring it. */
    const char *start = value;

    while (*start)
    {
        while (*start == ' ' || *start == '\t')
            start++;

        const char *end = strchr(start, ',');
        const char *tok_end = end ? end : start + strlen(start);

        const char *trimmed_end = tok_end;
        while (trimmed_end > start && (trimmed_end[-1] == ' ' || trimmed_end[-1] == '\t'))
            trimmed_end--;

        size_t tok_len = (size_t)(trimmed_end - start);

        if (tok_len == 0 || tok_len != 7 || bb_strncasecmp(start, "chunked", 7) != 0)
        {
            set_error(p, "unsupported Transfer-Encoding coding");
            return -1;
        }

        p->transfer_chunked = true;

        if (!end)
            break;
        start = end + 1;
    }

    if (!p->transfer_chunked)
    {
        set_error(p, "Transfer-Encoding did not resolve to chunked");
        return -1;
    }

    p->has_transfer_encoding = true;
    return 0;
}

/* --------------------------------------------------------------------- */
/* Body buffer                                                           */
/* --------------------------------------------------------------------- */

static int body_reserve(bb_http_parser_t *p, uint64_t needed_total)
{
    if (needed_total <= p->body_cap)
        return 0;

    uint64_t new_cap = p->body_cap ? p->body_cap * 2 : 4096;
    if (new_cap < needed_total)
        new_cap = needed_total;
    if (new_cap > BB_HTTP_MAX_BODY_SIZE)
        new_cap = BB_HTTP_MAX_BODY_SIZE;

    uint8_t *tmp = realloc(*active_body(p), (size_t)new_cap);
    if (!tmp)
    {
        set_error(p, "out of memory");
        return -1;
    }

    *active_body(p) = tmp;
    p->body_cap = new_cap;
    return 0;
}

static int body_append(bb_http_parser_t *p, const uint8_t *data, size_t len)
{
    if (len == 0)
        return 0;

    if (body_reserve(p, p->body_written + len) < 0)
        return -1;

    memcpy(*active_body(p) + p->body_written, data, len);
    p->body_written += len;
    *active_body_len(p) = (size_t)p->body_written;
    return 0;
}

/* --------------------------------------------------------------------- */
/* Header-section completion: decide what comes next                     */
/* --------------------------------------------------------------------- */

static int finish_headers(bb_http_parser_t *p)
{
    if (p->response_mode && ((p->response.status_code >= 100 && p->response.status_code < 200) || p->response.status_code == 204 || p->response.status_code == 304))
    {
        p->state = BB_HTTP_PARSE_STATE_COMPLETE;
        return 0;
    }
    if (p->has_content_length && p->has_transfer_encoding)
    {
        set_error(p, "ambiguous framing: both Content-Length and Transfer-Encoding present");
        return -1;
    }

    if (p->has_transfer_encoding)
    {
        p->state = BB_HTTP_PARSE_CHUNK_SIZE;
        reset_line(p, BB_HTTP_MAX_CHUNK_LINE);
        return 0;
    }

    if (p->has_content_length && p->content_length > 0)
    {
        if (body_reserve(p, p->content_length) < 0)
            return -1;
        p->state = BB_HTTP_PARSE_BODY_FIXED;
        return 0;
    }

    p->state = BB_HTTP_PARSE_STATE_COMPLETE;
    return 0;
}

/* --------------------------------------------------------------------- */
/* Chunk size line                                                       */
/* --------------------------------------------------------------------- */

static int parse_chunk_size_line(bb_http_parser_t *p, uint64_t *out_size)
{
    const uint8_t *buf = p->line_buf;
    size_t len = p->line_len;

    size_t i = 0;
    if (i == len)
    {
        set_error(p, "empty chunk-size line");
        return -1;
    }

    uint64_t val = 0;
    size_t digits = 0;

    while (i < len)
    {
        uint8_t c = buf[i];
        int digit;

        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F')
            digit = 10 + (c - 'A');
        else
            break;

        if (val > (UINT64_MAX >> 4))
        {
            set_error(p, "chunk size overflow");
            return -1;
        }
        val = (val << 4) | (uint64_t)digit;
        digits++;
        i++;
    }

    if (digits == 0)
    {
        set_error(p, "malformed chunk-size line");
        return -1;
    }

    /* Anything left must be a chunk-extension, introduced by ';'. We do
     * not need to interpret extensions, only ensure the size token isn't
     * followed by garbage. */
    if (i < len && buf[i] != ';')
    {
        set_error(p, "malformed chunk-size line");
        return -1;
    }

    if (val > BB_HTTP_MAX_CHUNK_SIZE)
    {
        set_error(p, "chunk size exceeds configured limit");
        return -1;
    }

    *out_size = val;
    return 0;
}

/* --------------------------------------------------------------------- */
/* Main incremental feed loop                                            */
/* --------------------------------------------------------------------- */

bb_http_parse_status_t bb_http_parser_feed(
    bb_http_parser_t *p,
    const uint8_t *data,
    size_t len)
{
    if (!p)
        return BB_HTTP_PARSE_ERROR;

    p->last_consumed = 0;

    if (p->state == BB_HTTP_PARSE_STATE_ERROR)
        return BB_HTTP_PARSE_ERROR;
    if (p->state == BB_HTTP_PARSE_STATE_COMPLETE)
        return BB_HTTP_PARSE_COMPLETE;

    if (len > 0 && !data)
    {
        set_error(p, "null buffer with non-zero length");
        return BB_HTTP_PARSE_ERROR;
    }

    const uint8_t *cursor = data;
    size_t remaining = len;

    for (;;)
    {
        switch (p->state)
        {
            case BB_HTTP_PARSE_REQUEST_LINE:
            {
                int r = consume_line(p, &cursor, &remaining);
                if (r == 0)
                    goto out_incomplete;
                if (r < 0)
                    goto out_error;

                if ((p->response_mode ? parse_response_line(p) : parse_request_line(p)) < 0)
                    goto out_error;

                reset_line(p, BB_HTTP_MAX_HEADER_SIZE);
                p->state = BB_HTTP_PARSE_HEADERS;
                break;
            }

            case BB_HTTP_PARSE_HEADERS:
            {
                int r = consume_line(p, &cursor, &remaining);
                if (r == 0)
                    goto out_incomplete;
                if (r < 0)
                    goto out_error;

                if (p->line_len == 0)
                {
                    if (finish_headers(p) < 0)
                        goto out_error;

                    if (p->state == BB_HTTP_PARSE_STATE_COMPLETE)
                        goto out_complete;

                    break;
                }

                const char *name = NULL;
                const char *value = NULL;
                size_t *header_count = NULL;
                bb_http_header_t **headers = active_headers(p, &header_count);
                if (parse_field_line(
                        p,
                        headers, header_count,
                        BB_HTTP_MAX_HEADER_COUNT,
                        &p->header_bytes_total, BB_HTTP_MAX_HEADER_SIZE,
                        &name, &value) < 0)
                {
                    goto out_error;
                }

                if (bb_strcasecmp(name, "content-length") == 0)
                {
                    if (apply_content_length(p, value) < 0)
                        goto out_error;
                }
                else if (bb_strcasecmp(name, "transfer-encoding") == 0)
                {
                    if (apply_transfer_encoding(p, value) < 0)
                        goto out_error;
                }

                reset_line(p, BB_HTTP_MAX_HEADER_SIZE);
                break;
            }

            case BB_HTTP_PARSE_BODY_FIXED:
            {
                uint64_t need = p->content_length - p->body_written;
                size_t take = (need < (uint64_t)remaining) ? (size_t)need : remaining;

                if (take > 0)
                {
                    if (body_append(p, cursor, take) < 0)
                        goto out_error;
                    cursor += take;
                    remaining -= take;
                }

                if (p->body_written >= p->content_length)
                {
                    p->state = BB_HTTP_PARSE_STATE_COMPLETE;
                    goto out_complete;
                }

                goto out_incomplete;
            }

            case BB_HTTP_PARSE_CHUNK_SIZE:
            {
                int r = consume_line(p, &cursor, &remaining);
                if (r == 0)
                    goto out_incomplete;
                if (r < 0)
                    goto out_error;

                uint64_t chunk_size = 0;
                if (parse_chunk_size_line(p, &chunk_size) < 0)
                    goto out_error;

                if (chunk_size > 0 &&
                    chunk_size > (uint64_t)BB_HTTP_MAX_BODY_SIZE - p->body_written)
                {
                    set_error(p, "chunked body exceeds configured body limit");
                    goto out_error;
                }

                if (chunk_size == 0)
                {
                    p->state = BB_HTTP_PARSE_TRAILERS;
                    reset_line(p, BB_HTTP_MAX_TRAILER_SIZE);
                }
                else
                {
                    p->chunk_remaining = chunk_size;
                    p->state = BB_HTTP_PARSE_CHUNK_DATA;
                }
                break;
            }

            case BB_HTTP_PARSE_CHUNK_DATA:
            {
                size_t take = (p->chunk_remaining < (uint64_t)remaining)
                                  ? (size_t)p->chunk_remaining
                                  : remaining;

                if (take > 0)
                {
                    if (body_append(p, cursor, take) < 0)
                        goto out_error;
                    cursor += take;
                    remaining -= take;
                    p->chunk_remaining -= take;
                }

                if (p->chunk_remaining > 0)
                    goto out_incomplete;

                p->state = BB_HTTP_PARSE_CHUNK_DATA_CRLF;
                reset_line(p, 0);
                break;
            }

            case BB_HTTP_PARSE_CHUNK_DATA_CRLF:
            {
                int r = consume_line(p, &cursor, &remaining);
                if (r == 0)
                    goto out_incomplete;
                if (r < 0)
                    goto out_error;

                if (p->line_len != 0)
                {
                    set_error(p, "malformed chunk terminator");
                    goto out_error;
                }

                p->state = BB_HTTP_PARSE_CHUNK_SIZE;
                reset_line(p, BB_HTTP_MAX_CHUNK_LINE);
                break;
            }

            case BB_HTTP_PARSE_TRAILERS:
            {
                int r = consume_line(p, &cursor, &remaining);
                if (r == 0)
                    goto out_incomplete;
                if (r < 0)
                    goto out_error;

                if (p->line_len == 0)
                {
                    p->state = BB_HTTP_PARSE_STATE_COMPLETE;
                    goto out_complete;
                }

                const char *name = NULL;
                const char *value = NULL;
                size_t *trailer_count = NULL;
                bb_http_header_t **trailers = active_trailers(p, &trailer_count);
                if (parse_field_line(
                        p,
                        trailers, trailer_count,
                        BB_HTTP_MAX_TRAILER_COUNT,
                        &p->trailer_bytes_total, BB_HTTP_MAX_TRAILER_SIZE,
                        &name, &value) < 0)
                {
                    goto out_error;
                }

                if (bb_strcasecmp(name, "content-length") == 0 ||
                    bb_strcasecmp(name, "transfer-encoding") == 0)
                {
                    set_error(p, "framing header not allowed in trailers");
                    goto out_error;
                }

                reset_line(p, BB_HTTP_MAX_TRAILER_SIZE);
                break;
            }

            case BB_HTTP_PARSE_STATE_COMPLETE:
                goto out_complete;

            case BB_HTTP_PARSE_STATE_ERROR:
                goto out_error;
        }
    }

out_incomplete:
    p->last_consumed = (size_t)(cursor - data);
    return BB_HTTP_PARSE_INCOMPLETE;

out_complete:
    p->last_consumed = (size_t)(cursor - data);
    return BB_HTTP_PARSE_COMPLETE;

out_error:
    p->last_consumed = (size_t)(cursor - data);
    if (p->state != BB_HTTP_PARSE_STATE_ERROR)
        set_error(p, "parse error");
    return BB_HTTP_PARSE_ERROR;
}
