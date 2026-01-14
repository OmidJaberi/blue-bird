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
int serialize_message(http_message_t *msg, char *buffer, int buffer_size)
{
    int written = snprintf(buffer, buffer_size,
                           "%s\r\n",
                           msg->start_line);
    
    for (int i = 0; i < msg->header_count; i++)
        written += snprintf(buffer + written, buffer_size - written,
                "%s: %s\r\n",
                msg->headers[i].name,
                msg->headers[i].value);

    int body_len = msg->body ? strlen(msg->body) : 0;
    // Conent_Length added here:
    written += snprintf(buffer + written, buffer_size - written,
            "Content-Length: %d\r\n\r\n",
            body_len);

    if (msg->body)
        written += snprintf(buffer + written, buffer_size - written,
                "%s", msg->body);

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