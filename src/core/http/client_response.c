#include "core/http/client_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void init_client_response(client_response_t *res)
{
    init_server_response(res);
}

void destroy_client_response(client_response_t *res)
{
    destroy_server_response(res);
}

const char *get_client_response_header(client_response_t *res, const char *name)
{
    return get_message_header(&res->msg, name);
}

int parse_client_response(const char *raw, client_response_t *res)
{
    if (!raw || !res)
        return -1;

    init_client_response(res);
    if (parse_message(raw, &res->msg) != 0)
        return -1;

    /* HTTP/1.1 200 OK */
    sscanf(res->msg.start_line, "HTTP/%*s %d", &res->status_code);

    return 0;
}

