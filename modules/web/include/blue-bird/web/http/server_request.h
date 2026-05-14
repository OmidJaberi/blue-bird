#ifndef BB_SERVER_REQUEST_H
#define BB_SERVER_REQUEST_H

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
} _bb_param_t;

typedef struct {
    char key[MAX_QUERY_PARAM_KEY];
    char value[MAX_QUERY_PARAM_VALUE];
} _bb_query_param_t;

typedef struct {
    bb_http_message_t msg;

    char method[METHOD_SIZE];
    char path[PATH_SIZE];
    char version[VERSION_SIZE];

    _bb_param_t params[MAX_PARAMS];
    int param_count;

    _bb_query_param_t query[MAX_QUERY_PARAMS];
    int query_count;

} bb_server_request_t;

void bb_server_request_init(bb_server_request_t *req);

int bb_server_request_parse(const char *raw, bb_server_request_t *req);

void bb_server_request_destroy(bb_server_request_t *req);

int bb_server_request_add_param(bb_server_request_t *req, const char *key, const char *value);

const char *bb_server_request_get_param(bb_server_request_t *req, const char *name);

int bb_server_request_add_query_param(bb_server_request_t *req, const char *key, const char *value);

const char *bb_server_request_get_query_param(bb_server_request_t *req, const char *key);

const char *bb_server_request_get_header(bb_server_request_t *req, const char *name);

#endif //BB_SERVER_REQUEST_H
