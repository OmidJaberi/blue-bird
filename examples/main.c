#include "core/network.h"
#include "core/router.h"
#include "core/http.h"
#include <unistd.h>
#include <string.h>

void hello_handler(Request *req, int client_fd)
{
    char *response = "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: 5\r\n"
                     "\r\n"
                     "Hello";
    write(client_fd, response, strlen(response));
}

void root_handler(Request *req, int client_fd)
{
    char *response = "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: 12\r\n"
                     "\r\n"
                     "Blue-Bird :)";
    write(client_fd, response, strlen(response));
}

int main()
{
    add_route("/", root_handler);
    add_route("/hello", hello_handler);

    init_server(8080);
    start_server();
    return 0;
}
