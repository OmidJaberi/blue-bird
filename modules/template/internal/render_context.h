#ifndef BB_TEMPLATE_RENDER_CONTEXT_H
#define BB_TEMPLATE_RENDER_CONTEXT_H

#include <blue-bird/utils/json.h>

typedef struct bb_render_context {
    bb_json_t current;
    struct bb_render_context *parent;
} bb_render_context_t;

#endif
