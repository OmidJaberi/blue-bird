#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#include <stddef.h>

#define MAX_HEADERS 50
#define MAX_HEADER_NAME 64
#define MAX_HEADER_VALUE 256

typedef struct {
    char name[MAX_HEADER_NAME];
    char value[MAX_HEADER_VALUE];
} header_t;

typedef struct {
    char *start_line;

    header_t headers[MAX_HEADERS];
    int header_count;

    char *body;
    size_t body_len;
} http_message_t;

void set_start_line(http_message_t *msg, const char *start_line);

const char *get_header(http_message_t *msg, const char *name);

void set_header(http_message_t *msg, const char *name, const char *value);

void set_body(http_message_t *msg, const char *body);


#endif //HTTP_MESSAGE_H