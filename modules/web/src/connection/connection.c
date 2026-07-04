#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

#include "connection/connection.h"

#define BB_CONNECTION_INITIAL_BUFFER_SIZE 4096

static int _bb_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        return -1;
    }
    return 0;
}

bb_connection_t *bb_connection_create(int fd)
{
    bb_connection_t *connection = malloc(sizeof(bb_connection_t));

    if (!connection)
    {
        return NULL;
    }

    connection->fd = fd;
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
    connection->write_data = NULL;
    connection->write_pending = false;

    return connection;
}

void bb_connection_destroy(bb_connection_t *connection)
{
    if (!connection)
    {
        return;
    }

    close(connection->fd);
    free(connection->buffer);

    while (connection->write_data)
    {
        write_buffer_t *next = connection->write_data->next;
        free(connection->write_data);
        connection->write_data = next;
    }

    free(connection);
}

int bb_connection_buffer_add(bb_connection_t *connection, char *buffer, size_t length)
{
    if (length == 0)
        return 0;

    write_buffer_t **write_data = &(connection->write_data);
    while (*write_data)
    {
        write_data = &((*write_data)->next);
    }
    *write_data = calloc(1, sizeof(write_buffer_t));
    if (!*write_data)
        return 1; // handle allocation failure appropriately
    
    (*write_data)->write_buffer = buffer;
    (*write_data)->write_length = length;
    (*write_data)->write_offset = 0;

    return 0;
}

bb_connection_t *bb_connection_serve(int port)
{
    int server_fd = -1;
    struct sockaddr_in address;
    int opt = 1;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        return NULL;
    }

    if (_bb_set_nonblocking(server_fd) != 0)
    {
        return NULL;
    }

    // Reuse port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        return NULL;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        return NULL;
    }

    // Listen
    if (listen(server_fd, 3) < 0)
    {
        return NULL;
    }

    bb_connection_t *connection = bb_connection_create(server_fd);
    if (!connection)
    {
        close(server_fd);
    }
    return connection;
}

bb_connection_t *bb_connection_accept(int server_fd)
{
    struct sockaddr_in address;

    socklen_t addrlen = sizeof(address);

    int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (client_fd < 0)
    {
        /*
        * Nonblocking socket:
        * no more pending connections.
        */
        return NULL;
    }

    if (_bb_set_nonblocking(client_fd) != 0)
    {
        return NULL;
    }

    bb_connection_t *connection = bb_connection_create(client_fd);

    if (!connection)
    {
        close(client_fd);
        return NULL;
    }
    return connection;
}

int _connect(const char *host, const char *port_str)
{
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0)
    {
        return -1;
    }

    int fd = -1;

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
            continue;

        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
        {
            break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    return fd;
}

bb_connection_t *bb_connection_connect(const char *host, const char *port_str)
{
    int fd = _connect(host, port_str);

    if (fd < 0)
    {
        return NULL;
    }

    bb_connection_t *connection = bb_connection_create(fd);
    if (!connection)
    {
        close(fd);
    }
    return connection;
}

bb_connection_t *bb_connection_connect_nonblocking(const char *host, const char *port_str)
{
    int fd = _connect(host, port_str);

    if (fd < 0)
    {
        return NULL;
    }

    if (_bb_set_nonblocking(fd) != 0)
    {
        return NULL;
    }

    bb_connection_t *connection = bb_connection_create(fd);
    if (!connection)
    {
        close(fd);
    }
    return connection;
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
                connection->fd,
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
    if (!connection)
    {
        return -1;
    }

    while (connection->write_data != NULL)
    {
        while (connection->write_data->write_offset < connection->write_data->write_length)
        {
            ssize_t n = send(
                connection->fd,
                connection->write_data->write_buffer + connection->write_data->write_offset,
                connection->write_data->write_length - connection->write_data->write_offset,
                0
            );
            if (n > 0)
            {
                connection->write_data->write_offset += n;
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                return 0;
            }
            return -1;
        }
        free(connection->write_data->write_buffer);
        connection->write_data->write_buffer = NULL;
        connection->write_data->write_length = 0;
        connection->write_data->write_offset = 0;

        write_buffer_t *next = connection->write_data->next;
        free(connection->write_data);
        connection->write_data = next;
    }
    return 1;
}
