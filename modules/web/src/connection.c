#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "connection.h"

#define BB_CONNECTION_INITIAL_BUFFER_SIZE 4096

bb_connection_t *bb_connection_create(struct bb_server *server, int client_fd)
{
    bb_connection_t *connection = malloc(sizeof(bb_connection_t));

    if (!connection)
    {
        return NULL;
    }

    connection->client_fd = client_fd;
    connection->server = server;
    connection->state = BB_CONNECTION_READING;

    // Read buffer
    connection->buffer = malloc(BB_CONNECTION_INITIAL_BUFFER_SIZE);
    if (!connection->buffer)
    {
        free(connection);
        return NULL;
    }

    connection->buffer_length = 0;
    connection->buffer_capacity = BB_CONNECTION_INITIAL_BUFFER_SIZE;

    // Write buffer
    connection->write_buffer = NULL;
    connection->write_length = 0;
    connection->write_offset = 0;

    bb_request_init_with_type(&connection->request, BB_SERVER_REQUEST);

    connection->response = bb_response_create();

    return connection;
}

void bb_connection_destroy(bb_connection_t *connection)
{
    if (!connection)
    {
        return;
    }

    close(connection->client_fd);
    free(connection->buffer);
    free(connection->write_buffer);

    bb_request_destroy(&connection->request);
    bb_response_destroy(connection->response);

    free(connection);
}

ssize_t bb_connection_read(bb_connection_t *connection)
{
    if (!connection)
    {
        return -1;
    }

    ssize_t total = 0;

    while (1)
    {
        // Ensure space
        if (connection->buffer_length == connection->buffer_capacity)
        {
            size_t new_capacity = connection->buffer_capacity * 2;
            char *tmp = realloc(connection->buffer, new_capacity);
            if (!tmp)
            {
                return -1;
            }
            connection->buffer = tmp;
            connection->buffer_capacity = new_capacity;
        }

        ssize_t n =
            read(
                connection->client_fd,
                connection->buffer +
                connection->buffer_length,
                connection->buffer_capacity -
                connection->buffer_length
            );

        if (n > 0)
        {
            connection->buffer_length += n;
            total += n;
            continue;
        }

        if (n == 0)
        {
            // Peer closed connection
            return total;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            break;
        }
        return -1;
    }
    return total;
}

ssize_t bb_connection_write(bb_connection_t *connection)
{
    while (connection->write_offset < connection->write_length)
    {
        ssize_t n = send(
            connection->client_fd,
            connection->write_buffer + connection->write_offset,
            connection->write_length - connection->write_offset,
            0
        );
        if (n > 0)
        {
            connection->write_offset += n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return 0;
        }
        return -1;
    }
    return 1;
}
