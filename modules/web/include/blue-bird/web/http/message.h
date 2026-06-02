#ifndef BB_HTTP_MESSAGE_H
#define BB_HTTP_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

typedef struct bb_http_message bb_http_message_t;

bb_http_message_t *bb_message_create(void);

const char *bb_message_get_start_line(bb_http_message_t *msg);
void bb_message_set_start_line(bb_http_message_t *msg, const char *start_line);

const char *bb_message_get_header(bb_http_message_t *msg, const char *name);
void bb_message_set_header(bb_http_message_t *msg, const char *name, const char *value);
int bb_message_get_header_count(bb_http_message_t *msg);

const char *bb_message_get_body(bb_http_message_t *msg);
void bb_message_set_body(bb_http_message_t *msg, const char *body);
int bb_message_get_body_len(bb_http_message_t *msg);

int bb_message_parse(const char *raw, bb_http_message_t *msg);
int bb_message_serialize(bb_http_message_t *msg, char **buffer, size_t *buffer_size);

void bb_message_destroy(bb_http_message_t *msg);


#ifdef __cplusplus
}
#endif

#endif //BB_HTTP_MESSAGE_H
