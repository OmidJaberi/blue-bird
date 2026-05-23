#include "string_builder.h"

#include <stdlib.h>
#include <string.h>

#define BB_STRING_BUILDER_INITIAL_CAPACITY 256

static int bb_string_builder_grow(bb_string_builder_t *sb, size_t required)
{
    while (sb->length + required + 1 > sb->capacity)
    {
        sb->capacity *= 2;
    }
    char *tmp = realloc(sb->data, sb->capacity);
    if (!tmp)
    {
        return -1;
    }
    sb->data = tmp;
    return 0;
}

int bb_string_builder_init(bb_string_builder_t *sb)
{
    if (!sb)
    {
        return -1;
    }
    sb->capacity = BB_STRING_BUILDER_INITIAL_CAPACITY;
    sb->length = 0;
    sb->data = malloc(sb->capacity);
    if (!sb->data)
    {
        return -1;
    }
    sb->data[0] = '\0';
    return 0;
}

int bb_string_builder_append_n(bb_string_builder_t *sb, const char *text, size_t len)
{
    if (!sb || !text)
    {
        return -1;
    }
    if (sb->length + len + 1 > sb->capacity)
    {
        if (bb_string_builder_grow(sb, len) != 0)
        {
            return -1;
        }
    }
    memcpy(sb->data + sb->length, text, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
    return 0;
}

int bb_string_builder_append(bb_string_builder_t *sb, const char *text)
{
    return bb_string_builder_append_n(sb, text, strlen(text));
}

void bb_string_builder_destroy(bb_string_builder_t *sb)
{
    if (!sb)
    {
        return;
    }
    free(sb->data);
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}
