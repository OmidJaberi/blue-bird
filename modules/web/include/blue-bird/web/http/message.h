#ifndef BB_HTTP_MESSAGE_H
#define BB_HTTP_MESSAGE_H

#include <stddef.h>

typedef struct {
    char *name;
    char *value;
} _bb_message_header_t;

typedef struct {
    char *start_line; // Fixed size ?

    _bb_message_header_t *headers;
    int header_count;

    char *body;
    size_t body_len;
} bb_http_message_t;

void bb_message_init(bb_http_message_t *msg);

void bb_message_set_start_line(bb_http_message_t *msg, const char *start_line);

const char *bb_message_get_header(bb_http_message_t *msg, const char *name);

void bb_message_set_header(bb_http_message_t *msg, const char *name, const char *value);

void bb_message_set_body(bb_http_message_t *msg, const char *body);

int bb_message_parse(const char *raw, bb_http_message_t *msg);

int bb_message_serialize(bb_http_message_t *msg, char **buffer, int *buffer_size);

void bb_message_destroy(bb_http_message_t *msg);

#endif //BB_HTTP_MESSAGE_H
