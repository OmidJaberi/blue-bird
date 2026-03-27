#include "core/http.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

ssize_t read_http_message(int fd, char **out_buf)
{
    size_t cap = 4096;
    size_t len = 0;
    ssize_t n;
    char *buf = malloc(cap);
    if (!buf) return -1;

    ssize_t header_end = -1;
    size_t content_length = 0;

    while (1)
    {
        if (len == cap)
        {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                return -1;
            }
            buf = tmp;
        }

        n = read(fd, buf + len, cap - len);
        if (n <= 0)
            break;

        len += n;

        if (header_end == -1)
        {
            buf[len] = 0;  // ensure strstr works safely
            char *p = strstr(buf, "\r\n\r\n");
            if (p)
            {
                header_end = (p - buf) + 4;

                // parse Content-Length
                char *cl = strcasestr(buf, "Content-Length:");
                if (cl)
                {
                    cl += strlen("Content-Length:");
                    content_length = strtoul(cl, NULL, 10);
                }
            }
        }

        if (header_end != -1)
        {
            if (len >= header_end + content_length)
                break;
        }
    }

    if (len == cap)
    {
        char *tmp = realloc(buf, cap + 1);
        if (!tmp) {
            free(buf);
            return -1;
        }
        buf = tmp;
    }

    buf[len] = '\0';
    *out_buf = buf;
    return len;
}