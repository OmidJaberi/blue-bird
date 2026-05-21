#include "template_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} bb_string_builder_t;

static int bb_sb_init(bb_string_builder_t *sb)
{
    sb->capacity = 256;
    sb->length = 0;

    sb->data = malloc(sb->capacity);

    if (!sb->data) {
        return -1;
    }

    sb->data[0] = '\0';

    return 0;
}

static int bb_sb_append(bb_string_builder_t *sb, const char *text)
{
    size_t len = strlen(text);

    while (sb->length + len + 1 > sb->capacity)
    {
        sb->capacity *= 2;

        char *tmp = realloc(sb->data, sb->capacity);

        if (!tmp)
        {
            return -1;
        }

        sb->data = tmp;
    }

    memcpy(sb->data + sb->length, text, len + 1);
    sb->length += len;
    return 0;
}

static bb_json_t *bb_template_lookup(bb_json_t *ctx, const char *path)
{
    char *copy = strdup(path);

    if (!copy)
    {
        return NULL;
    }

    char *token = strtok(copy, ".");

    bb_json_t *current = ctx;

    while (token && current)
    {
        current = bb_json_object_get_value(current, token);
        token = strtok(NULL, ".");
    }

    free(copy);
    return current;
}

static char *bb_json_to_string(bb_json_t *json)
{
    char *buf = NULL;
    int size = 0;

    if (json == NULL)
        return NULL;

    if (json->type == BB_JSON_TEXT)
    {
        buf = malloc(json->size + 1);
        if (buf == NULL)
            return NULL;

        memcpy(buf, json->value.text_val, json->size);
        buf[json->size] = '\0';
    }
    else
    {
        bb_json_serialize(json, &buf, &size);
    }

    return buf;
}

char *bb_template_render(const bb_template_t *tpl, bb_json_t *context, bb_error_t *err)
{
    (void) err;

    bb_string_builder_t sb;

    if (bb_sb_init(&sb) != 0)
    {
        return NULL;
    }

    bb_template_node_t *node = tpl->ast.head;

    while (node)
    {
        if (node->type == BB_TEMPLATE_NODE_TEXT)
        {
            if (bb_sb_append(&sb, node->value) != 0)
            {
                free(sb.data);
                return NULL;
            }
        }
        else if (node->type == BB_TEMPLATE_NODE_VARIABLE)
        {
            bb_json_t *value = bb_template_lookup(context, node->value);

            if (value)
            {
                char *tmp = bb_json_to_string(value);

                if (!tmp)
                {
                    free(sb.data);
                    return NULL;
                }

                if (bb_sb_append(&sb, tmp) != 0)
                {
                    free(tmp);
                    free(sb.data);
                    return NULL;
                }

                free(tmp);
            }
        }

        node = node->next;
    }

    return sb.data;
}
