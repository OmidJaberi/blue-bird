#ifndef BB_RESPONSE_H
#define BB_RESPONSE_H

#ifdef __cplusplus
extern "C" {
#endif


#include "message.h"

typedef struct bb_response bb_response_t;

bb_response_t *bb_response_create(void);

void bb_response_destroy(bb_response_t *res);

int bb_response_set_status(bb_response_t *res, int code);

int bb_response_get_status(bb_response_t *res);

void bb_response_set_header(bb_response_t *res, const char *name, const char *value);

const char *bb_response_get_header(bb_response_t *res, const char *name);

void bb_response_set_body(bb_response_t *res, char *body);

bb_http_message_t *bb_response_get_message(bb_response_t *res);

static inline const char *bb_response_get_body(bb_response_t *res)
{
    return bb_message_get_body(bb_response_get_message(res));
}

int bb_response_serialize(bb_response_t *res, char **buffer, size_t *size);

int bb_response_parse(const char *raw, bb_response_t *res);


#ifdef __cplusplus
}
#endif

#endif //BB_RESPONSE_H
