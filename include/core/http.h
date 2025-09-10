#ifndef HTTP_H
#define HTTP_H

#define METHOD_SIZE 8
#define PATH_SIZE 256
#define VERSION_SIZE 16

typedef struct {
    char method[METHOD_SIZE];
    char path[PATH_SIZE];
    char version[VERSION_SIZE];
} Request;

typedef struct {
    int status;
    char *body;
} Response;

int parse_request(const char *raw, Request *req);

#endif // HTTP_H
