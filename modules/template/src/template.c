#include "blue-bird/template/template.h"

#include <blue-bird/utils/asset.h>

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
    return bb_template_parse_internal(source, tpl);
}

bb_error_t bb_template_parse_file(const char *path, bb_template_t **tpl)
{
    char *source;

    bb_error_t err = bb_asset_text_read_all(path, &source, NULL);

    err = bb_template_parse(source, tpl);

    free(source);

    return err;
}

bb_error_t bb_template_render(const bb_template_t *tpl, bb_json_t *context, char **buf)
{
    if (!tpl || !context)
    {
        return BB_ERROR(BB_ERR_NULL, "Empty template, or context.");
    }
    return bb_template_render_internal(tpl, context, buf);
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
