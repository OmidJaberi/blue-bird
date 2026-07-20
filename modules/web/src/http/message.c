#include "blue-bird/web/http/message.h"
#include "blue-bird/utils/encoding.h"

#include <blue-bird/utils/platform.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char *name;
    char *value;
} _bb_message_header_t;

struct bb_http_message {
    char *start_line; // Fixed size ?

    _bb_message_header_t *headers;
    int header_count;

    char *body;
    size_t body_len;
};

bb_http_message_t *bb_message_create(void)
{
    bb_http_message_t *msg = calloc(1, sizeof(bb_http_message_t));
    return msg;
}

void bb_message_destroy(bb_http_message_t *msg)
{
    if (!msg)
    {
        return;
    }
    bb_message_reset(msg);
    free(msg);
}

void bb_message_reset(bb_http_message_t *msg)
{
    if (!msg)
    {
        return;
    }
    for (int i = 0; i < msg->header_count; i++)
    {
        free(msg->headers[i].name);
        free(msg->headers[i].value);
    }
    msg->header_count = 0;
    if (msg->headers)
    {
        free(msg->headers);
        msg->headers = NULL;
    }
    if (msg->body)
    {
        free(msg->body);
        msg->body = NULL;
        msg->body_len = 0;
    }
    if (msg->start_line)
    {
        free(msg->start_line);
        msg->start_line = NULL;
    }
}

const char *bb_message_get_start_line(bb_http_message_t *msg)
{
    return msg->start_line;
}

void bb_message_set_start_line(bb_http_message_t *msg, const char *start_line)
{
    if (!msg) return;

    // Free any previous start line
    if (msg->start_line) {
        free(msg->start_line);
        msg->start_line = NULL;
    }

    if (!start_line) return;

    size_t len = strlen(start_line);
    msg->start_line = (char *)malloc(len + 1);
    if (!msg->start_line) return; // malloc failed

    memcpy(msg->start_line, start_line, len);
    msg->start_line[len] = '\0';
}

const char *bb_message_get_header(bb_http_message_t *msg, const char *name)
{
    for (int i = 0; i < msg->header_count; i++)
        if (strcmp(msg->headers[i].name, name) == 0)
            return msg->headers[i].value;
    return NULL;
}

void bb_message_set_header(bb_http_message_t *msg, const char *name, const char *value)
{
    void *tmp = realloc(msg->headers, (msg->header_count + 1)* sizeof(*msg->headers));
    if (!tmp)
        return;

    msg->headers = tmp;
    msg->headers[msg->header_count].name = strdup(name);
    if (!msg->headers[msg->header_count].name)
    {
        return;
    }
    msg->headers[msg->header_count].value = strdup(value);
    if (!msg->headers[msg->header_count].value)
    {
        free(msg->headers[msg->header_count].name);
        return;
    }
    msg->header_count++;
}

int bb_message_get_header_count(bb_http_message_t *msg)
{
    return msg->header_count;
}

const char *bb_message_get_body(bb_http_message_t *msg)
{
    return msg->body;
}

void bb_message_set_body(bb_http_message_t *msg, const char *body)
{
    if (!msg) return;

    // Free previous body if any
    if (msg->body) {
        free(msg->body);
        msg->body = NULL;
        msg->body_len = 0;
    }

    if (!body) return;

    size_t len = strlen(body);
    msg->body = (char *)malloc(len + 1);
    if (!msg->body) return; // malloc failed, just leave empty

    memcpy(msg->body, body, len);
    msg->body[len] = '\0';
    msg->body_len = len;
}

static int _parse_header(const char **raw, char **name_buf, char **value_buf)
{
    const char *colon = strchr(*raw, ':');
    if (!colon) return -1;  // Malformed Header (missing ':')

    const char *end = strstr(*raw, "\r\n");
    if (!end) return -1;    // Malformed Header (no CRLF)

    size_t name_len = colon - *raw;
    size_t value_len = end - (colon + 1);

    // copy name
    *name_buf = malloc(name_len + 1);
    if (!*name_buf) return -1;
    strncpy(*name_buf, *raw, name_len);
    (*name_buf)[name_len] = '\0';

    // Skip leading space in value
    const char *val_start = colon + 1;
    while (*val_start == ' ' && value_len > 0)
    {
        val_start++;
        value_len--;
    }
    // copy value
    *value_buf = malloc(value_len + 1);
    if (!*value_buf)
    {
        free(*name_buf);
        *name_buf = NULL;
        return -1;
    }
    strncpy(*value_buf, val_start, value_len);
    (*value_buf)[value_len] = '\0';

    *raw = end + 2; // next line
    return 0;
}

static int parse_body(bb_http_message_t *msg, const char *raw)
{
    const char *body_start = strstr(raw, "\r\n\r\n");
    if (!body_start) return 0;

    body_start += 4; // skip "\r\n\r\n"
    size_t available = strlen(body_start);

    const char *content_length = bb_message_get_header(msg, "Content-Length");
    size_t body_len = available;

    if (content_length)
    {
        long declared = atol(content_length);
        if (declared < 0)
            return -1; // malformed Content-Length

        // Never trust a declared length beyond what we actually received.
        body_len = (size_t)declared < available ? (size_t)declared : available;
    }

    if (body_len <= 0)
        return 0;

    char *body_buf = (char *)malloc(body_len + 1);
    if (!body_buf) return -1;

    memcpy(body_buf, body_start, body_len);
    body_buf[body_len] = '\0';

    const char *ctype = bb_message_get_header(msg, "Content-Type");
    if (ctype && strcasecmp(ctype, "application/x-www-form-urlencoded") == 0)
        bb_decode_percent(body_buf, 1);

    // Store into message
    bb_message_set_body(msg, body_buf);
    free(body_buf);

    return 0;
}

int bb_message_get_body_len(bb_http_message_t *msg)
{
    return msg->body_len;
}

int bb_message_parse(const char *raw, bb_http_message_t *msg)
{
    if (!raw || !msg) return -1;

    bb_message_reset(msg);

    // Parse start line
    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;

    char line[512];
    size_t len = line_end - raw;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    strncpy(line, raw, len);
    line[len] = '\0';

    // Set the start line in bb_http_message_t
    bb_message_set_start_line(msg, line);

    // Parse headers
    const char *header_start = line_end + 2;

    while (*header_start && !(header_start[0] == '\r' && header_start[1] == '\n'))
    {
        char *name_buf = NULL;
        char *value_buf = NULL;

        if (_parse_header(&header_start, &name_buf, &value_buf) < 0)
        {
            if (name_buf) free(name_buf);
            if (value_buf) free(value_buf);
            return -1;
        }

        bb_message_set_header(msg, name_buf, value_buf);

        if (name_buf) free(name_buf);
        if (value_buf) free(value_buf);
    }

    // Parse Body
    return parse_body(msg, raw);
}

int bb_message_serialize(bb_http_message_t *msg, char **buffer, size_t *buffer_size)
{
    size_t needed = 0;

    int body_len = msg->body ? (int)strlen(msg->body) : 0;

    needed += msg->start_line ? strlen(msg->start_line) + 2 : 2; // \r\n

    // Content_Length added here:
    char len_buf[256];
    snprintf(len_buf, 256, "%d", body_len);
    bb_message_set_header(msg, "Content-Length", len_buf);

    for (int i = 0; i < msg->header_count; i++)
    {
        needed += strlen(msg->headers[i].name) + strlen(msg->headers[i].value) + 4; // ": \r\n"
    }

    needed += 2; // final \r\n
    needed += body_len;

    if (!buffer)
    {
        *buffer_size = needed;
        return 0;
    }

    *buffer = malloc(needed + 1);
    if (!*buffer)
        return -1;

    char *p = *buffer;

    // start line
    size_t n = strlen(msg->start_line);
    memcpy(p, msg->start_line, n);
    p += n;
    *p++ = '\r';
    *p++ = '\n';

    // headers
    for (int i = 0; i < msg->header_count; i++)
    {
        n = strlen(msg->headers[i].name);
        memcpy(p, msg->headers[i].name, n);
        p += n;

        *p++ = ':';
        *p++ = ' ';

        n = strlen(msg->headers[i].value);
        memcpy(p, msg->headers[i].value, n);
        p += n;

        *p++ = '\r';
        *p++ = '\n';
    }

    // final CRLF (THIS is what Node was complaining about)
    *p++ = '\r';
    *p++ = '\n';

    // body
    if (body_len > 0)
    {
        memcpy(p, msg->body, body_len);
        p += body_len;
    }

    *buffer_size = (size_t)(p - *buffer);
    (*buffer)[*buffer_size] = '\0';
    return 0;
}
