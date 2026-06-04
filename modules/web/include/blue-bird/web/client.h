#ifndef BB_CLIENT_H
#define BB_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/http/request.h"
#include "blue-bird/web/http/response.h"
#include "blue-bird/error/error.h"

typedef struct bb_client bb_client_t;

bb_client_t *bb_client_create(void);
void bb_client_destroy(bb_client_t *client);

bb_request_t *bb_client_request(bb_client_t *client);
bb_response_t *bb_client_response(bb_client_t *client);

bb_error_t bb_client_connect(bb_client_t *client, const char *host, int port);
bb_error_t bb_client_send(bb_client_t *client);
bb_error_t bb_client_receive(bb_client_t *client);
void bb_client_close(bb_client_t *client);


#ifdef __cplusplus
}
#endif

#endif //BB_CLIENT_H
