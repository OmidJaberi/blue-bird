#include "core/http/message.h"

void set_message_start_line(http_message_t *msg, const char *start_line) {}

const char *get_message_header(http_message_t *msg, const char *name) {}

void set_message_header(http_message_t *msg, const char *name, const char *value) {}

void set_message_body(http_message_t *msg, const char *body) {}

void destroy_message(http_message_t *msg)
{
    for (int i = 0; i < msg->header_count; i++)
    {
        free(msg->headers[i].name);
        free(msg->headers[i].value);
    }
    free(msg->headers);

    if (msg->body)
    {
        free(msg->body);
        msg->body = NULL;
        msg->body_len = 0;
    }
}