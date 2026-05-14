#include "blue-bird/web/http/client_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void bb_client_response_init(bb_client_response_t *res)
{
    bb_server_response_init(res);
}

void bb_client_response_destroy(bb_client_response_t *res)
{
    bb_server_response_destroy(res);
}

const char *bb_client_response_get_header(bb_client_response_t *res, const char *name)
{
    return bb_message_get_header(&res->msg, name);
}

int bb_client_response_parse(const char *raw, bb_client_response_t *res)
{
    if (!raw || !res)
        return -1;

    bb_client_response_init(res);
    if (bb_message_parse(raw, &res->msg) != 0)
        return -1;

    /* HTTP/1.1 200 OK */
    sscanf(res->msg.start_line, "HTTP/%*s %d", &res->status_code);

    return 0;
}
