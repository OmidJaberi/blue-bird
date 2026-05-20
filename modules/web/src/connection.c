#include <stdlib.h>
#include <unistd.h>

#include "blue-bird/web/connection.h"

bb_connection_t *bb_connection_create(struct bb_server *server, int client_fd)
{
    bb_connection_t *connection = malloc(sizeof(bb_connection_t));

    if (!connection)
    {
        return NULL;
    }

    connection->client_fd = client_fd;
    connection->server = server;
    connection->buffer = NULL;
    connection->buffer_length = 0;

    bb_request_init_with_type(&connection->request, BB_SERVER_REQUEST);

    bb_response_init(&connection->response);

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

    bb_request_destroy(&connection->request);
    bb_response_destroy(&connection->response);

    free(connection);
}
