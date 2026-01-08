#include "core/http/client_response.h"
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

int parse_client_response(const char *raw, client_response_t *res)
{
    if (!raw || !res)
        return -1;

    init_client_response(res);

    /* ---- status line ---- */
    const char *line_end = strstr(raw, "\r\n");
    if (!line_end)
        return -1;

    char status_line[256];
    size_t len = line_end - raw;
    if (len >= sizeof(status_line))
        len = sizeof(status_line) - 1;

    memcpy(status_line, raw, len);
    status_line[len] = '\0';

    set_message_start_line(&res->msg, status_line);

    /* HTTP/1.1 200 OK */
    sscanf(status_line, "HTTP/%*s %d", &res->status_code);

    /* ---- headers ---- */
    const char *p = line_end + 2;
    while (*p && !(p[0] == '\r' && p[1] == '\n'))
    {
        const char *colon = strchr(p, ':');
        const char *end = strstr(p, "\r\n");
        if (!colon || !end)
            return -1;

        char name[128];
        char value[512];

        size_t name_len = colon - p;
        if (name_len >= sizeof(name))
            name_len = sizeof(name) - 1;
        memcpy(name, p, name_len);
        name[name_len] = '\0';

        const char *val = colon + 1;
        while (*val == ' ')
            val++;

        size_t val_len = end - val;
        if (val_len >= sizeof(value))
            val_len = sizeof(value) - 1;
        memcpy(value, val, val_len);
        value[val_len] = '\0';

        set_message_header(&res->msg, name, value);
        p = end + 2;
    }

    /* ---- body ---- */
    const char *body = strstr(raw, "\r\n\r\n");
    if (!body)
        return 0;

    body += 4;
    if (*body)
        set_message_body(&res->msg, body);

    return 0;
}

