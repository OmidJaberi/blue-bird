#ifndef BB_FI_INTERCEPT_H
#define BB_FI_INTERCEPT_H

/*
 * Included first by every fi_*.c wrapper, before the real source file.
 *
 * The system headers that declare the allocator come first so their
 * prototypes are not renamed; the macros apply to everything after.
 * bb_runtime_watch_fd_ex is renamed before runtime.h is parsed so that the
 * static inline bb_runtime_watch_fd() wrapper in that header is intercepted
 * too.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fault_inject.h"

#define malloc(size)       bb_fi_malloc(size)
#define calloc(n, size)    bb_fi_calloc((n), (size))
#define realloc(ptr, size) bb_fi_realloc((ptr), (size))
#define bb_strdup          bb_fi_strdup
#define bb_runtime_watch_fd_ex bb_fi_watch_fd_ex

#include <blue-bird/runtime/runtime.h>
#include <blue-bird/utils/platform.h>

#endif /* BB_FI_INTERCEPT_H */
