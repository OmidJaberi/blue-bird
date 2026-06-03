#ifndef BB_CONNECTION_H
#define BB_CONNECTION_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/http/request.h"
#include "blue-bird/web/http/response.h"

#include <stddef.h>
#include <unistd.h>

struct bb_server;

typedef enum {
    BB_CONNECTION_READING,
    BB_CONNECTION_WRITING,
    BB_CONNECTION_CLOSED
} bb_connection_state_t;

typedef struct bb_connection {
    int client_fd;
    struct bb_server *server;

    bb_connection_state_t state;

    // Read buffer
    char *buffer;
    size_t buffer_length;
    size_t buffer_capacity;

    // Write buffer
    char *write_buffer;
    size_t write_length;
    size_t write_offset;

    bb_request_t *request;
    bb_response_t *response;
} bb_connection_t;

bb_connection_t *bb_connection_create(struct bb_server *server, int client_fd);
void bb_connection_destroy(bb_connection_t *connection);
ssize_t bb_connection_read(bb_connection_t *connection);
ssize_t bb_connection_write(bb_connection_t *connection);


#ifdef __cplusplus
}
#endif

#endif
