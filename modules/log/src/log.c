#include "log/log.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

Logger default_logger;

static const char *level_str(LogLevel level)
{
    switch(level)
    {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

void logger_log(Logger *logger, LogLevel level, const char *fmt, ...)
{
    if (!logger || !logger->write || level > logger->level) return;

    va_list args;
    va_start(args, fmt);
    logger->write(logger, level, fmt, args);
    va_end(args);
}

void logger_vlog(Logger *logger, LogLevel level, const char *fmt, va_list args)
{
    if (!logger || !logger->write || level > logger->level) return;

    logger->write(logger, level, fmt, args);
}

void logger_close(Logger *logger)
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
