#ifndef BB_CONNECTION_H
#define BB_CONNECTION_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <unistd.h>

typedef enum {
    BB_CONNECTION_READING,
    BB_CONNECTION_WRITING,
    BB_CONNECTION_CLOSED
} bb_connection_state_t;

typedef struct bb_connection {
    int fd;

    bb_connection_state_t state;

    // Read buffer
    char *buffer;
    size_t buffer_length;
    size_t buffer_capacity;

    // Write buffer
    char *write_buffer;
    size_t write_length;
    size_t write_offset;

    void *userdata;
} bb_connection_t;

bb_connection_t *bb_connection_create(int client_fd);
void bb_connection_destroy(bb_connection_t *connection);

bb_connection_t *bb_connection_serve(int port);
bb_connection_t *bb_connection_accept(int server_fd);
bb_connection_t *bb_connection_connect(const char *host, const char *port_str);
bb_connection_t *bb_connection_connect_nonblocking(const char *host, const char *port_str);
ssize_t bb_connection_read(bb_connection_t *connection);
ssize_t bb_connection_write(bb_connection_t *connection);


#ifdef __cplusplus
}
#endif

#endif
