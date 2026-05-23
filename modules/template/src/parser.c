#include "template_internal.h"
#include "ast.h"

#include "blue-bird/utils/json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct {
    const char *src;
    size_t pos;
    size_t len;
} bb_template_parser_t;


static int bb_parser_starts_with(bb_template_parser_t *p, const char *str)
{
    size_t n = strlen(str);
    if (p->pos + n > p->len)
    {
        return 0;
    }
    return strncmp(p->src + p->pos, str, n) == 0;
}


static char *bb_parser_read_until(bb_template_parser_t *p, const char *delimiter)
{
    size_t start = p->pos;
    size_t delim_len = strlen(delimiter);
    while (p->pos < p->len && !bb_parser_starts_with(p, delimiter))
    {
        p->pos++;
    }
    if (p->pos >= p->len)
    {
        return NULL;
    }
    size_t size = p->pos - start;
    char *out = malloc(size + 1);

    if (!out)
    {
        return NULL;
    }

    memcpy(out, p->src + start, size);
    out[size] = '\0';
    p->pos += delim_len;

    return out;
}

static int bb_parser_append_text(bb_template_node_list_t *list, const char *text, size_t len)
{
    if (len == 0)
    {
        return 0;
    }
    char *tmp = malloc(len + 1);

    if (!tmp)
    {
        return -1;
    }
    memcpy(tmp, text, len);
    tmp[len] = '\0';

    bb_template_node_t *node = bb_template_node_create(BB_TEMPLATE_NODE_TEXT, tmp);
    free(tmp);

    if (!node)
    {
        return -1;
    }

    bb_template_node_list_append(list, node);
    return 0;
}

static int bb_parser_parse_nodes(bb_template_parser_t *p, bb_template_node_list_t *list, const char *closing_tag);

static int bb_parser_parse_section(bb_template_parser_t *p, bb_template_node_list_t *list)
{
    p->pos += 3;
    char *name = bb_parser_read_until(p, "}}");
    if (!name)
    {
        return -1;
    }

    bb_template_node_t *node = bb_template_node_create(BB_TEMPLATE_NODE_SECTION, name);

    free(name);

    if (!node)
    {
        return -1;
    }

    bb_template_node_list_t children = {0};
    if (bb_parser_parse_nodes(p, &children, node->value) != 0)
    {
        bb_template_node_destroy(node);
        return -1;
    }
    node->children = children.head;
    bb_template_node_list_append(list, node);
    return 0;
}


static int bb_parser_parse_conditional(bb_template_parser_t *p, bb_template_node_list_t *list)
{
    p->pos += 3;
    char *name = bb_parser_read_until(p, "}}");
    if (!name)
    {
        return -1;
    }
    bb_template_node_t *node = bb_template_node_create(BB_TEMPLATE_NODE_CONDITIONAL, name);
    free(name);
    if (!node)
    {
        return -1;
    }
    bb_template_node_list_t children = {0};
    if (bb_parser_parse_nodes(p, &children, node->value) != 0)
    {
        bb_template_node_destroy(node);
        return -1;
    }
    node->children = children.head;
    bb_template_node_list_append(list, node);
    return 0;
}


static int bb_parser_parse_variable(bb_template_parser_t *p, bb_template_node_list_t *list)
{
    p->pos += 2;
    char *name = bb_parser_read_until(p, "}}");
    if (!name)
    {
        return -1;
    }
    bb_template_node_t *node = bb_template_node_create(BB_TEMPLATE_NODE_VARIABLE, name);
    free(name);
    if (!node)
    {
        return -1;
    }
    bb_template_node_list_append(list, node);
    return 0;
}


static int bb_parser_parse_comment(bb_template_parser_t *p)
{
    p->pos += 3;
    char *tmp = bb_parser_read_until(p, "}}");
    free(tmp);
    return 0;
}


static int bb_parser_parse_nodes(bb_template_parser_t *p, bb_template_node_list_t *list, const char *closing_tag)
{
    size_t text_start = p->pos;
    while (p->pos < p->len)
    {
        // Escaped opening delimiter.
        if (bb_parser_starts_with(p, "\\{{"))
        {
            if (bb_parser_append_text(list, p->src + text_start, p->pos - text_start) != 0)
            {
                return -1;
            }
            // Emit literal {{
            if (bb_parser_append_text(list, "{{", 2) != 0)
            {
                return -1;
            }
            p->pos += 3;
            text_start = p->pos;
            continue;
        }
        // Closing section.
        if (bb_parser_starts_with(p, "{{/"))
        {
            // Flush pending text BEFORE consuming closing tag.
            if (bb_parser_append_text(list, p->src + text_start, p->pos - text_start) != 0)
            {
                return -1;
            }
            p->pos += 3;
            char *name = bb_parser_read_until(p, "}}");
            if (!name)
            {
                return -1;
            }
            int matches = closing_tag && strcmp(name, closing_tag) == 0;
            free(name);
            return matches ? 0 : -1;
        }
        // Section.
        if (bb_parser_starts_with(p, "{{#"))
        {
            if (bb_parser_append_text(list, p->src + text_start, p->pos - text_start) != 0)
            {
                return -1;
            }
            if (bb_parser_parse_section(p, list) != 0)
            {
                return -1;
            }
            text_start = p->pos;
            continue;
        }
        // Conditional.
        if (bb_parser_starts_with(p, "{{?"))
        {
            if (bb_parser_append_text(list, p->src + text_start, p->pos - text_start) != 0)
            {
                return -1;
            }
            if (bb_parser_parse_conditional(p, list) != 0)
            {
                return -1;
            }
            text_start = p->pos;
            continue;
        }
        // Comment.
        if (bb_parser_starts_with(p, "{{!"))
        {
            if (bb_parser_append_text(list, p->src + text_start, p->pos - text_start) != 0)
            {
                return -1;
            }
            if (bb_parser_parse_comment(p) != 0)
            {
                return -1;
            }
            text_start = p->pos;
            continue;
        }
        // Variable.
        if (bb_parser_starts_with(p, "{{"))
        {
            if (bb_parser_append_text(list, p->src + text_start, p->pos - text_start) != 0)
            {
                return -1;
            }
            if (bb_parser_parse_variable(p, list) != 0)
            {
                return -1;
            }
            text_start = p->pos;
            continue;
        }
        p->pos++;
    }
    // Flush trailing text.
    if (bb_parser_append_text(list, p->src + text_start, p->pos - text_start) != 0)
    {
        return -1;
    }
    /*
     * If we expected a closing tag
     * but hit EOF:
     */
    if (closing_tag)
    {
        return -1;
    }
    return 0;
}

bb_template_t *bb_template_parse_internal(const char *source, bb_error_t *err)
{
    (void) err;
    bb_template_t *tpl = calloc(1, sizeof(bb_template_t));
    if (!tpl)
    {
        return NULL;
    }
    bb_template_parser_t parser = {
        .src = source,
        .pos = 0,
        .len = strlen(source)
    };
    if (bb_parser_parse_nodes(&parser, &tpl->nodes, NULL) != 0)
    {
        bb_template_destroy(tpl);
        return NULL;
    }
    return tpl;
}
