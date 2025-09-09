#ifndef HTTP_H
#define HTTP_H

typedef struct {
    char *method;
    char *path;
} Request;

typedef struct {
    int status;
    char *body;
} Response;

#endif // HTTP_H
