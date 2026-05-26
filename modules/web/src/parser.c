#include "blue-bird/web/http.h"

#include <string.h>
#include <stdlib.h>

int bb_http_request_complete(const char *buffer, size_t length)
{
    if (!buffer || length == 0)
    {
        return 0;
    }

    // Find end of headers
    const char *headers_end = strstr(buffer, "\r\n\r\n");
    if (!headers_end)
    {
        return 0;
    }

    size_t header_length = (headers_end - buffer) + 4;

    // Check Content-Length
    size_t content_length = 0;

    char *cl = strcasestr(buffer, "Content-Length:");
    if (cl)
    {
        cl += strlen("Content-Length:");
        content_length = strtoul(cl, NULL, 10);
    }

    return length >= header_length + content_length;
}
