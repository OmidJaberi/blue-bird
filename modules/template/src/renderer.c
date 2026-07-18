#include "renderer.h"
#include "render_context.h"
#include "string_builder.h"

#include "blue-bird/utils/json.h"

#include <stdlib.h>
#include <string.h>

static bb_json_t *bb_template_lookup(bb_render_context_t *ctx, const char *path)
{
    char *copy = strdup(path);
    if (!copy)
    {
        return NULL;
    }
    bb_json_t *result = NULL;
    bb_render_context_t *current_ctx = ctx;
    while (current_ctx && !result)
    {
        bb_json_t *current = current_ctx->current;
        char *token = strtok(copy, ".");
        while (token && current)
        {
            current = bb_json_object_get_value(current, token);
            token = strtok(NULL, ".");
        }
        if (current)
        {
            result = current;
            break;
        }
        /*
         * Restart tokenization
         * for parent context.
         */
        strcpy(copy, path);
        current_ctx = current_ctx->parent;
    }
    free(copy);
    return result;
}

static int bb_template_is_truthy(bb_json_t *value)
{
    if (!value)
    {
        return 0;
    }
    switch (bb_json_get_type(value))
    {
        case BB_JSON_NULL:
            return 0;
        case BB_JSON_BOOL:
            return bb_json_get_value_bool(value);
        case BB_JSON_INT:
            return bb_json_get_value_integer(value) != 0;
        case BB_JSON_REAL:
            return bb_json_get_value_real(value) != 0.0;
        case BB_JSON_TEXT:
        {
            const char *s = bb_json_get_value_text(value);
            return s && s[0] != '\0';
        }
        case BB_JSON_ARRAY:
            return bb_json_get_size(value) > 0;
        case BB_JSON_OBJECT:
            return 1;
        default:
            return 0;
    }
}

static int bb_template_render_value(bb_string_builder_t *sb, bb_json_t *value)
{
    if (!value)
    {
        return 0;
    }

    // Strings render raw.
    if (bb_json_get_type(value) == BB_JSON_TEXT)
    {
        return bb_string_builder_append(sb, bb_json_get_value_text(value));
    }

    // Everything else:
    char *tmp;
    int size;
    if (BB_FAILED(bb_json_serialize(value, &tmp, &size)) || !tmp)
    {
        return -1;
    }

    int rc = bb_string_builder_append(sb, tmp);
    free(tmp);
    return rc;
}


static int bb_template_render_nodes(bb_template_node_t *node, bb_render_context_t *ctx, bb_string_builder_t *sb)
{
    while (node)
    {
        switch (node->type)
        {
            case BB_TEMPLATE_NODE_TEXT:
            {
                if (bb_string_builder_append(sb, node->value) != 0)
                {
                    return -1;
                }
                break;
            }
            case BB_TEMPLATE_NODE_VARIABLE:
            {
                bb_json_t *value = bb_template_lookup(ctx, node->value);
                if (bb_template_render_value(sb, value) != 0)
                {
                    return -1;
                }
                break;
            }
            case BB_TEMPLATE_NODE_SECTION:
            {
                bb_json_t *value = bb_template_lookup(ctx, node->value);
                if (value && bb_json_get_type(value) == BB_JSON_ARRAY)
                {
                    size_t count = bb_json_get_size(value);
                    for (size_t i = 0; i < count; i++)
                    {
                        bb_json_t *item = bb_json_array_get_index(value, i);
                        bb_render_context_t child_ctx = {
                            .current = item,
                            .parent = ctx
                        };
                        if (bb_template_render_nodes(node->children, &child_ctx, sb) != 0)
                        {
                            return -1;
                        }
                    }
                }
                break;
            }
            case BB_TEMPLATE_NODE_CONDITIONAL:
            {
                bb_json_t *value = bb_template_lookup(ctx, node->value);
                if (bb_template_is_truthy(value))
                {
                    if (bb_template_render_nodes(node->children, ctx, sb) != 0)
                    {
                        return -1;
                    }
                }
                break;
            }
            case BB_TEMPLATE_NODE_COMMENT:
                break;
        }
        node = node->next_list;
    }
    return 0;
}

bb_error_t bb_template_render_internal(const bb_template_t *tpl, bb_json_t *context, char **buf)
{
    bb_string_builder_t sb;
    if (bb_string_builder_init(&sb) != 0)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to create string builder");
    }
    bb_render_context_t root_ctx = {
        .current = context,
        .parent = NULL
    };
    if (bb_template_render_nodes(tpl->nodes.head, &root_ctx, &sb) != 0)
    {
        bb_string_builder_destroy(&sb);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to render");
    }
    *buf = sb.data;
    return BB_SUCCESS();
}
