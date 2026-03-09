#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "core/http.h"
#include "error/error.h"

typedef struct {
    int sock_fd;
} bb_client_t;

BBError http_client_connect(bb_client_t *client, const char *host, int port);
BBError http_client_send(bb_client_t *client, request_t *req);
BBError http_client_receive(bb_client_t *client, response_t *res);
void http_client_close(bb_client_t *client);

#endif //HTTP_CLIENT_H
