#ifndef BB_ASSET_H
#define BB_ASSET_H

#include <stddef.h>

#include "blue-bird/error/error.h"

bb_error_t bb_asset_resolve_path(const char *path, char *buffer, size_t size);
bb_error_t bb_asset_text_read_all(const char *path, char **buffer, size_t *length);

#endif /* BB_ASSET_H */
