#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>
#include <core/http/message.h>

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

} request_t;

int parse_request(const char *raw, request_t *req);

void destroy_request(request_t *req);

const char *get_param(request_t *req, const char *name);

int add_query_param(request_t *req, const char *key, const char *value);

const char *get_query_param(request_t *req, const char *key);

const char *get_header(request_t *req, const char *name);

#endif // REQUEST_H
