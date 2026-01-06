#include "core/http/clinet_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void init_client_response(client_response_t *res)
{
    res->status_code = 200;
    res->status_text = strdup("OK");
    init_message(&res->msg);
}

void destroy_client_response(client_response_t *res)
{
    free(res->status_text);
    destroy_message(&res->msg);
}

const char *get_client_header(client_response_t *res, const char *name)
{
    return get_message_header(&res->msg, name);
}
