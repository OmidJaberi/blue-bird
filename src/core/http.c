#include "core/http.h"
#include <stdio.h>
#include <string.h>

int parse_request(const char *raw, Request *req)
{
    if (!raw || !req) return -1;

    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;

    char line[512];
    size_t len = line_end - raw;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    strncpy(line, raw, len);
    line[len] = '\0';

    if (sscanf(line, "%7s %255s %15s", req->method, req->path, req->version) != 3) {
        return -1;
    }

    return 0;
}
