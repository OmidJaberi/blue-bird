#ifndef BB_TEMPLATE_RENDERER_H
#define BB_TEMPLATE_RENDERER_H

#include "template_internal.h"

#include <blue-bird/error/error.h>
#include <blue-bird/utils/json.h>

char *bb_template_render_internal(const bb_template_t *tpl, bb_json_t *context, bb_error_t *err);

#endif
