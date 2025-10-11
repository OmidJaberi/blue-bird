#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>

#define METHOD_SIZE 8
#define PATH_SIZE 256
#define VERSION_SIZE 16

#define MAX_PARAMS 10
#define MAX_PARAM_NAME 64
#define MAX_PARAM_VALUE 256

#define MAX_QUERY_PARAMS 10
#define MAX_QUERY_PARAM_KEY 64
#define MAX_QUERY_PARAM_VALUE 256

#define MAX_HEADERS 50
#define MAX_HEADER_NAME 64
#define MAX_HEADER_VALUE 256

typedef struct {
    char name[MAX_PARAM_NAME];
    char value[MAX_PARAM_VALUE];
} Param;

typedef struct {
    char key[MAX_QUERY_PARAM_KEY];
    char value[MAX_QUERY_PARAM_VALUE];
} QueryParam;

typedef struct {
    char name[MAX_HEADER_NAME];
    char value[MAX_HEADER_VALUE];
} Header;

typedef struct {
    char method[METHOD_SIZE];
    char path[PATH_SIZE];
    char version[VERSION_SIZE];

    Param params[MAX_PARAMS];
    int param_count;

    QueryParam query[MAX_QUERY_PARAMS];
    int query_count;

    Header headers[MAX_HEADERS];
    int header_count;

    char *body;
    size_t body_len;

} Request;

int parse_request(const char *raw, Request *req);

void destroy_request(Request *req);

const char *get_param(Request *req, const char *name);

int add_query_param(Request *req, const char *key, const char *value);

const char *get_query_param(Request *req, const char *key);

const char *get_header(Request *req, const char *name);

#endif // REQUEST_H
