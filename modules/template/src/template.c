#include "blue-bird/template/template.h"

#include "parser.h"
#include "renderer.h"
#include "template_internal.h"

#include <stdlib.h>

bb_template_t *bb_template_parse(const char *source, bb_error_t *err)
{
    if (!source)
    {
        return NULL;
    }
    return bb_template_parse_internal(source, err);
}

char *bb_template_render(const bb_template_t *tpl, bb_json_t context, bb_error_t *err)
{
    if (!tpl || !context)
    {
        return NULL;
    }
    return bb_template_render_internal(tpl, context, err);
}

void bb_template_destroy(bb_template_t *tpl)
{
    if (!tpl)
    {
        return;
    }
    bb_template_node_list_destroy(&tpl->nodes);
    free(tpl);
}
