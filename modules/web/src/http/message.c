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
    for (int i = 0; i < msg->header_count; i++)
    {
        if (strcmp(msg->headers[i].name, name) == 0)
        {
            char *new_value = bb_strdup(value);
            if (!new_value)
                return;

            free(msg->headers[i].value);
            msg->headers[i].value = new_value;
            return;
        }
    }
    void *tmp = realloc(msg->headers, (msg->header_count + 1)* sizeof(*msg->headers));
    if (!tmp)
        return;

    msg->headers = tmp;
    msg->headers[msg->header_count].name = bb_strdup(name);
    if (!msg->headers[msg->header_count].name)
    {
        return;
    }
    msg->headers[msg->header_count].value = bb_strdup(value);
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
    if (!body)
    {
        bb_message_set_body_data(msg, NULL, 0);
        return;
    }
    bb_message_set_body_data(msg, body, strlen(body));
}

void bb_message_set_body_data(bb_http_message_t *msg, const void *body, size_t body_len)
{
    if (!msg) return;

    free(msg->body);
    msg->body = NULL;
    msg->body_len = 0;

    if (body_len == 0) return;
    if (!body) return;

    msg->body = malloc(body_len + 1);
    if (!msg->body) return;

    memcpy(msg->body, body, body_len);
    msg->body[body_len] = '\0';
    msg->body_len = body_len;
}

int bb_message_get_body_len(bb_http_message_t *msg)
{
    return msg->body_len;
}

int bb_message_serialize(bb_http_message_t *msg, char **buffer, size_t *buffer_size)
{
    size_t needed = 0;

    size_t body_len = msg->body_len;

    needed += msg->start_line ? strlen(msg->start_line) + 2 : 2; // \r\n

    // Content_Length added here:
    char len_buf[256];
    snprintf(len_buf, 256, "%zu", body_len);
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
