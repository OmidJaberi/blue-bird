#ifndef RESPONSE_H
#define RESPONSE_H

typedef struct {
    int status_code;
    char *status_text;

    struct {
        char *name;
        char *value;
    } *headers;
    int header_count;

    char *body;
} Response;

void init_response(Response *res);
void destroy_response(Response *res);
int set_status(Response *res, int code);
void set_header(Response *res, const char *name, const char *value);
void set_body(Response *res, char *body);
int serialize_response(Response *res, char *buffer, int buffer_size);

int send_response(int sock_fd, Response *res);

#endif // RESPONSE_H
