#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#include <stddef.h>

typedef struct {
    char *start_line;

    struct {
        char *name;
        char *value;
    } headers;
    int header_count;

    char *body;
    size_t body_len;
} http_message_t;

#endif //HTTP_MESSAGE_H