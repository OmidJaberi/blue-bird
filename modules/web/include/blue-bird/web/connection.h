#ifndef BB_CONNECTION_H
#define BB_CONNECTION_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/http/request.h"
#include "blue-bird/web/http/response.h"

struct bb_server;

typedef struct bb_connection {
    int client_fd;
    struct bb_server *server;

    char *buffer;
    size_t buffer_length;

    bb_request_t request;
    bb_response_t response;
} bb_connection_t;

bb_connection_t *bb_connection_create(struct bb_server *server, int client_fd);
void bb_connection_destroy(bb_connection_t *connection);


#ifdef __cplusplus
}
#endif

#endif
