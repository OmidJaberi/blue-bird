#ifndef RESPONSE_H
#define RESPONSE_H

typedef struct {
    int status;
    char *headers;
    char *body;
} Response;

int create_response(Response *res, int status, const char *content);

int send_response(int sock_fd, Response *res);

#endif // RESPONSE_H
