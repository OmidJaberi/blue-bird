#ifndef HTTP_H
#define HTTP_H

#define METHOD_SIZE 8
#define PATH_SIZE 256
#define VERSION_SIZE 16

#define MAX_BODY 1024

typedef struct {
    char method[METHOD_SIZE];
    char path[PATH_SIZE];
    char version[VERSION_SIZE];
} Request;

typedef struct {
    int status;
    char *headers;
    char *body;
} Response;

int parse_request(const char *raw, Request *req);

int create_response(Response *res, int status, const char *content);

int send_response(int sock_fd, Response *res);

#endif // HTTP_H
