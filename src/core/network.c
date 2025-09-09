#include "core/network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

static int server_fd;

int init_server(int port)
{
    struct sockaddr_in address;
    int opt = 1;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Reuse port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Blue-Bird server initialized on port %d\n", port);
    return 0;
}

void start_server()
{
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[3000] = {0};

    printf("Blue-Bird server started.\n");
    while (1)
    {
        // Accept new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
        {
            perror("accept failed");
            continue;
        }

        // Read request (not parsed, yet)
        read(new_socket, buffer, sizeof(buffer));
        printf("Received request:\n%s\n", buffer);

        // Send response (hardcoded, for now)
        char *response = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 20\r\n"
                         "\r\n"
                         "Hello from Blue-Bird!\n";
        write(new_socket, response, strlen(response));

        close(new_socket);
    }
}
