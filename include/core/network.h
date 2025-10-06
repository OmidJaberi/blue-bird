#ifndef NETWORK_H
#define NETWORK_H

typedef struct {
    int server_fd;
} Server;

int init_server(Server *server, int port);
void start_server(Server *server);
void destroy_server(Server *server);

#endif // NETWORK_H
