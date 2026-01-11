#ifndef SERVER_REQUEST_H
#define SERVER_REQUEST_H

#include <stddef.h>
#include "message.h"

#define METHOD_SIZE 8
#define PATH_SIZE 256
#define VERSION_SIZE 16

#define MAX_PARAMS 10
#define MAX_PARAM_NAME 64
#define MAX_PARAM_VALUE 256

#define MAX_QUERY_PARAMS 10
#define MAX_QUERY_PARAM_KEY 64
#define MAX_QUERY_PARAM_VALUE 256

typedef struct {
    char name[MAX_PARAM_NAME];
    char value[MAX_PARAM_VALUE];
} param_t;

typedef struct {
    char key[MAX_QUERY_PARAM_KEY];
    char value[MAX_QUERY_PARAM_VALUE];
} query_param_t;

typedef struct {
    http_message_t msg;

    char method[METHOD_SIZE];
    char path[PATH_SIZE];
    char version[VERSION_SIZE];

    param_t params[MAX_PARAMS];
    int param_count;

    query_param_t query[MAX_QUERY_PARAMS];
    int query_count;

} server_request_t;

int parse_server_request(const char *raw, server_request_t *req);

void destroy_server_request(server_request_t *req);

const char *get_server_request_param(server_request_t *req, const char *name);

int add_server_request_query_param(server_request_t *req, const char *key, const char *value);

const char *get_server_request_query_param(server_request_t *req, const char *key);

const char *get_server_request_header(server_request_t *req, const char *name);

#endif // SERVER_REQUEST_H
