#ifndef BB_CLIENT_REQUEST_H
#define BB_CLIENT_REQUEST_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include "message.h"

typedef struct {
    bb_http_message_t msg;
    
    char *method;
    char *url;
    char *host;
    int port;
} bb_client_request_t;

void bb_client_request_init(bb_client_request_t *req);
void bb_client_request_set_method(bb_client_request_t *req, const char *method);
void bb_client_request_set_url(bb_client_request_t *req, const char *url);
void bb_client_request_set_header(bb_client_request_t *req, const char *name, const char *value);
void bb_client_request_set_body(bb_client_request_t *req, char *body);
void bb_client_request_destroy(bb_client_request_t *req);


#ifdef __cplusplus
}
#endif

#endif //BB_CLIENT_REQUEST_H
