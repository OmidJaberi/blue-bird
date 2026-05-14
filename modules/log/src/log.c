#include "blue-bird/log/log.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

bb_logger_t default_logger;

void bb_logger_log(bb_logger_t *logger, bb_log_level_t level, const char *fmt, ...)
{
    if (!logger || !logger->write || level > logger->level) return;

    va_list args;
    va_start(args, fmt);
    logger->write(logger, level, fmt, args);
    va_end(args);
}

void bb_logger_vlog(bb_logger_t *logger, bb_log_level_t level, const char *fmt, va_list args)
{
    if (!logger || !logger->write || level > logger->level) return;

    logger->write(logger, level, fmt, args);
}

void bb_logger_close(bb_logger_t *logger)
{
    if (!logger) return;

    if (logger->userdata)
    {
        // For persist backend we may need to close the store
        // For console backend, nothing to do
        // Persist-specific closing handled in backend
        logger->userdata = NULL;
    }
    logger->write = NULL;
}
