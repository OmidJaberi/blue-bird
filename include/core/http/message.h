#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#include <stddef.h>

typedef struct {
    char *name;
    char *value;
} header_t;

typedef struct {
    char *start_line; // Fixed size ?

    header_t *headers;
    int header_count;

    char *body;
    size_t body_len;
} http_message_t;

void init_message(http_message_t *msg);

void set_message_start_line(http_message_t *msg, const char *start_line);

const char *get_message_header(http_message_t *msg, const char *name);

void set_message_header(http_message_t *msg, const char *name, const char *value);

void set_message_body(http_message_t *msg, const char *body);

void destroy_message(http_message_t *msg);

#endif //HTTP_MESSAGE_H
