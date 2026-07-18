#include <string.h>
#include <stdlib.h>

#include <blue-bird/utils/platform.h>

static const char *find_bytes(const char *buf, size_t len,
                              const char *needle, size_t needle_len)
{
    if (needle_len == 0 || len < needle_len)
        return NULL;

    for (size_t i = 0; i <= len - needle_len; i++)
    {
        if (memcmp(buf + i, needle, needle_len) == 0)
            return buf + i;
    }

    return NULL;
}

static size_t parse_content_length(const char *buf, size_t header_len)
{
    const char *end = buf + header_len;
    const char *p = buf;

    while (p < end)
    {
        // find line end
        const char *line_end = memchr(p, '\n', end - p);
        if (!line_end)
            break;

        size_t line_len = line_end - p;
        if (line_len >= 15 && strncasecmp(p, "Content-Length:", 15) == 0)
        {
            const char *v = p + 15;
            while (v < line_end && (*v == ' ' || *v == '\t'))
                v++;
            return strtoul(v, NULL, 10);
        }
        p = line_end + 1;
    }

    return 0;
}

int bb_http_message_complete(const char *buf, size_t len)
{
    if (len < 4)
        return 0;

    const char *hdr_end = find_bytes(buf, len, "\r\n\r\n", 4);
    if (!hdr_end)
        return 0;

    size_t header_len = (hdr_end - buf) + 4;

    size_t content_length = parse_content_length(buf, header_len);

    // No body case (GET/HEAD/etc or missing header)
    if (content_length == 0)
        return len >= header_len;

    return len >= header_len + content_length;
}
