#ifndef BB_UTILS_DIR_H
#define BB_UTILS_DIR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ---------- Directory listing ---------- */

/*
 * Lists the entries of the directory at `path`.
 *
 *  - Returns entry NAMES (not paths), sorted with strcmp(), so the order
 *    is the same on every platform and filesystem.
 *  - "." and ".." are never included.
 *  - Files, subdirectories and everything else are all listed; callers
 *    filter by name.
 *  - `path` may end with '/' or '\' (or not).
 *
 * Returns 0 on success (an empty directory gives *out_count == 0 and
 * *out_names == NULL), or -1 if the directory can't be opened or read.
 * On success the caller releases the result with bb_dir_list_free().
 */
int bb_dir_list(const char *path, char ***out_names, size_t *out_count);

/* Frees a list returned by bb_dir_list(). NULL-safe. */
void bb_dir_list_free(char **names, size_t count);

#ifdef __cplusplus
}
#endif

#endif //BB_UTILS_DIR_H
