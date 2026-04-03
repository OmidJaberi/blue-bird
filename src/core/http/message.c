#include "core/http/message.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void init_message(http_message_t *msg)
{
    msg->start_line = NULL;
    msg->headers = NULL;
    msg->header_count = 0;
    msg->body = NULL;
    msg->body_len = 0;
}

void set_message_start_line(http_message_t *msg, const char *start_line)
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

const char *get_message_header(http_message_t *msg, const char *name)
{
    for (int i = 0; i < msg->header_count; i++)
        if (strcmp(msg->headers[i].name, name) == 0)
            return msg->headers[i].value;
    return NULL;
}

void set_message_header(http_message_t *msg, const char *name, const char *value)
{
    msg->headers = realloc(msg->headers, (msg->header_count + 1)* sizeof(*msg->headers));
    msg->headers[msg->header_count].name = strdup(name);
    msg->headers[msg->header_count].value = strdup(value);
    msg->header_count++;
}

void set_message_body(http_message_t *msg, const char *body)
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

static int parse_header(const char **raw, char **name_buf, char **value_buf)
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
    if (!*value_buf) return -1;
    strncpy(*value_buf, val_start, value_len);
    (*value_buf)[value_len] = '\0';

    *raw = end + 2; // next line
    return 0;
}

static int parse_body(http_message_t *msg, const char *raw)
{
    const char *body_start = strstr(raw, "\r\n\r\n");
    if (!body_start) return 0;

    body_start += 4; // skip "\r\n\r\n"
    const char *content_length = get_message_header(msg, "Content-Length");
    size_t body_len = content_length ? atoi(content_length) : strlen(body_start);

    if (body_len <= 0)
        return 0;

    char *body_buf = (char *)malloc(body_len + 1);
    if (!body_buf) return -1;

    memcpy(body_buf, body_start, body_len);
    body_buf[body_len] = '\0';

    // const char *ctype = get_message_header(msg, "Content-Type");
    // if (ctype && strcasecmp(ctype, "application/x-www-form-urlencoded") == 0)
    //     url_decode(body_buf, 1);

    // Store into message
    set_message_body(msg, body_buf);
    free(body_buf);

    return 0;
}

int parse_message(const char *raw, http_message_t *msg)
{
    if (!raw || !msg) return -1;

    init_message(msg);

    // Parse start line
    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;

    char line[512];
    size_t len = line_end - raw;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    strncpy(line, raw, len);
    line[len] = '\0';

    // Set the start line in http_message_t
    set_message_start_line(msg, line);

    // Parse headers
    const char *header_start = line_end + 2;

    while (*header_start && !(header_start[0] == '\r' && header_start[1] == '\n'))
    {
        char *name_buf = NULL;
        char *value_buf = NULL;

        if (parse_header(&header_start, &name_buf, &value_buf) < 0)
        {
            if (name_buf) free(name_buf);
            if (value_buf) free(value_buf);
            return -1;
        }

        set_message_header(msg, name_buf, value_buf);

        if (name_buf) free(name_buf);
        if (value_buf) free(value_buf);
    }

    // Parse Body
    return parse_body(msg, raw);
}

int serialize_message(http_message_t *msg, char **buffer, int *buffer_size)
{
    if (buffer)
    {
        *buffer_size = serialize_message(msg, NULL, NULL) + 1;
        *buffer = malloc(*buffer_size);
        if (!*buffer)
            return -1;
    }
    else
    {
        // Conent_Length added here:
        int body_len = msg->body ? strlen(msg->body) : 0;
        char len_buf[256];
        snprintf(len_buf, 256, "%d", body_len);
        set_message_header(msg, "Content-Length", len_buf);
    }

    // Start Line:
    int written = buffer ?
                    snprintf(*buffer, *buffer_size, "%s\r\n", msg->start_line)
                    : strlen(msg->start_line) + 2;

    // Headers:
    for (int i = 0; i < msg->header_count; i++)
        written += buffer ?
                    snprintf(*buffer + written, *buffer_size - written, "%s: %s\r\n", msg->headers[i].name, msg->headers[i].value)
                    : strlen(msg->headers[i].name) + strlen(msg->headers[i].value) + 4;
    if (msg->header_count > 0)
        written += buffer ?
                    snprintf(*buffer + written, *buffer_size - written, "\r\n")
                    : 2;

    // Body:
    if (msg->body)
        written += buffer ?
                    snprintf(*buffer + written, *buffer_size - written, "%s", msg->body)
                    : strlen(msg->body);

    return written;
}

void destroy_message(http_message_t *msg)
{
    for (int i = 0; i < msg->header_count; i++)
    {
        free(msg->headers[i].name);
        free(msg->headers[i].value);
    }

    if (msg->headers)
    {
        free(msg->headers);
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