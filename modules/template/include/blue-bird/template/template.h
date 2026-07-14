#ifndef BB_TEMPLATE_TEMPLATE_H
#define BB_TEMPLATE_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif


#include <blue-bird/error/error.h>
#include <blue-bird/utils/json.h>

typedef struct bb_template bb_template_t;

bb_error_t bb_template_parse(const char *source, bb_template_t **tpl);
bb_error_t bb_template_parse_file(const char *path, bb_template_t **tpl);
bb_error_t bb_template_render(const bb_template_t *tpl, bb_json_t *context, char **buf);
void bb_template_destroy(bb_template_t *tpl);


#ifdef __cplusplus
}
#endif

#endif
