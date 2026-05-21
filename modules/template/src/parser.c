#include "template_internal.h"
#include "ast.h"

#include "blue-bird/utils/json.h"

#include <stdlib.h>
#include <string.h>

static char *bb_template_substr(const char *src, size_t start, size_t len)
{
    char *out = malloc(len + 1);

    if (!out)
    {
        return NULL;
    }

    memcpy(out, src + start, len);
    out[len] = '\0';

    return out;
}

static int bb_template_append_text(bb_template_ast_t *ast, const char *text, size_t len)
{
    char *tmp = bb_template_substr(text, 0, len);

    if (!tmp)
    {
        return -1;
    }

    bb_template_node_t *node = bb_template_node_create(BB_TEMPLATE_NODE_TEXT, tmp);
    free(tmp);

    if (!node)
    {
        return -1;
    }

    bb_template_ast_append(ast, node);

    return 0;
}

static int bb_template_append_variable(bb_template_ast_t *ast, const char *text, size_t len)
{
    char *tmp = bb_template_substr(text, 0, len);

    if (!tmp)
    {
        return -1;
    }

    bb_template_node_t *node = bb_template_node_create(BB_TEMPLATE_NODE_VARIABLE, tmp);

    free(tmp);

    if (!node)
    {
        return -1;
    }

    bb_template_ast_append(ast, node);

    return 0;
}

bb_template_t *bb_template_parse(const char *source, bb_error_t *err)
{
    (void) err;

    bb_template_t *tpl = calloc(1, sizeof(bb_template_t));

    if (!tpl)
    {
        return NULL;
    }

    size_t i = 0;
    size_t text_start = 0;

    while (source[i])
    {

        if (
            source[i] == '\\' &&
            source[i + 1] == '{' &&
            source[i + 2] == '{'
        ) {

            /*
            * Flush text before escape.
            */
            if (i > text_start)
            {
                if (bb_template_append_text(&tpl->ast, source + text_start, i - text_start) != 0)
                {
                    bb_template_destroy(tpl);
                    return NULL;
                }
            }

            /*
            * Emit literal {{
            */
            if (bb_template_append_text(&tpl->ast, "{{", 2) != 0)
            {
                bb_template_destroy(tpl);
                return NULL;
            }

            /*
            * Skip:
            * \{{
            */
            i += 3;

            text_start = i;
            continue;
        }

        if (
            source[i] == '{' &&
            source[i + 1] == '{'
        ) {
            if (i > text_start)
            {
                if (bb_template_append_text(&tpl->ast, source + text_start, i - text_start) != 0)
                {
                    bb_template_destroy(tpl);
                    return NULL;
                }
            }

            i += 2;

            size_t var_start = i;

            while (
                source[i] &&
                !(source[i] == '}' &&
                  source[i + 1] == '}')
            ) {
                i++;
            }

            if (!source[i])
            {
                bb_template_destroy(tpl);
                return NULL;
            }

            if (bb_template_append_variable(&tpl->ast, source + var_start, i - var_start) != 0)
            {
                bb_template_destroy(tpl);
                return NULL;
            }

            i += 2;
            text_start = i;
            continue;
        }

        i++;
    }

    if (i > text_start)
    {
        if (bb_template_append_text(&tpl->ast, source + text_start, i - text_start) != 0)
        {
            bb_template_destroy(tpl);
            return NULL;
        }
    }

    return tpl;
}
