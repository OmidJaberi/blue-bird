#ifndef BB_TEMPLATE_STRING_BUILDER_H
#define BB_TEMPLATE_STRING_BUILDER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} bb_string_builder_t;

int bb_string_builder_init(bb_string_builder_t *sb);
int bb_string_builder_append(bb_string_builder_t *sb, const char *text);
int bb_string_builder_append_n(bb_string_builder_t *sb, const char *text, size_t len);
void bb_string_builder_destroy(bb_string_builder_t *sb);

#endif
