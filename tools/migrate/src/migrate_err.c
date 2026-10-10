#include "migrate_err.h"

#include <stdarg.h>
#include <stdio.h>

void bb_migrate_err_set(bb_migrate_err_t *err, const char *fmt, ...)
{
    if (!err)
        return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->msg, sizeof err->msg, fmt, ap);
    va_end(ap);
}
