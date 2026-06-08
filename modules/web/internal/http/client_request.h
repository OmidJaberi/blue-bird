#ifndef BB_CLIENT_REQUEST_H
#define BB_CLIENT_REQUEST_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include "blue-bird/web/http/message.h"

typedef struct {
    bb_http_message_t *msg;
    
    char *method;
    char *url;

    char *scheme;
    char *host;
    int port;
    char *path;
} bb_client_request_t;

void bb_client_request_init(bb_client_request_t *req);
void bb_client_request_destroy(bb_client_request_t *req);
void bb_client_request_reset(bb_client_request_t *req);

void bb_client_request_set_method(bb_client_request_t *req, const char *method);
void bb_client_request_set_url(bb_client_request_t *req, const char *url);

const char *bb_client_request_get_url(bb_client_request_t *req);
const char *bb_client_request_get_scheme(bb_client_request_t *req);
const char *bb_client_request_get_host(bb_client_request_t *req);
int bb_client_request_get_port(bb_client_request_t *req);
const char *bb_client_request_get_path(bb_client_request_t *req);

#ifdef __cplusplus
}
#endif

#endif //BB_CLIENT_REQUEST_H
