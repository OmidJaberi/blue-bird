#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "core/http.h"
#include "error/error.h"

typedef struct {
    int sock_fd;
} HttpClient;

BBError http_client_connect(HttpClient *client, const char *host, int port);
BBError http_client_send(HttpClient *client, request_t *req);
BBError http_client_receive(HttpClient *client, response_t *res);
void http_client_close(HttpClient *client);

#endif //HTTP_CLIENT_H
