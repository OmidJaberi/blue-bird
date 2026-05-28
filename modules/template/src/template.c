#include "blue-bird/template/template.h"

#include "parser.h"
#include "renderer.h"
#include "template_internal.h"

#include <stdlib.h>

bb_error_t bb_template_parse(const char *source, bb_template_t **tpl)
{
    if (!source)
    {
        return BB_ERROR(BB_ERR_NULL, "Empty source.");
    }
    *tpl = bb_template_parse_internal(source, NULL); // temporary, without error
    return BB_SUCCESS();
}

bb_error_t bb_template_render(const bb_template_t *tpl, bb_json_t *context, char **buf)
{
    if (!tpl || !context)
    {
        return BB_ERROR(BB_ERR_NULL, "Empty template, or context.");
    }
    *buf = bb_template_render_internal(tpl, context, NULL); // temporary, without error
    return BB_SUCCESS();
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
