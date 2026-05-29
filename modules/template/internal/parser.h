#ifndef BB_TEMPLATE_PARSER_H
#define BB_TEMPLATE_PARSER_H

#include "template_internal.h"

#include <blue-bird/error/error.h>

bb_error_t bb_template_parse_internal(const char *source, bb_template_t **tpl);

#endif
