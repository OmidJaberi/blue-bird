#include "blue-bird/log/log.h"
#include "blue-bird/log/console_logger.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

/* Zero-initialized: level=BB_LOG_LEVEL_ERROR, write=NULL, userdata=NULL.
 * The NULL write callback is what triggers the lazy auto-init below. */
bb_logger_t default_logger;

/* Auto-init: the first time anything logs through default_logger without it
 * having been explicitly configured, stand up a sane console backend
 * (INFO level, ERROR/WARN to stderr, everything else to stdout) so
 * BB_LOG_INFO(...) etc. work with zero setup. Explicitly assigning to
 * default_logger (or calling bb_logger_init_console/persist on it) before
 * first use still takes priority, since that leaves ->write non-NULL. */
static void ensure_default_logger_initialized(bb_logger_t *logger)
{
    if (logger == &default_logger && default_logger.write == NULL)
    {
        bb_logger_init_console(&default_logger, BB_LOG_LEVEL_INFO, NULL);
    }
}

void bb_logger_log(bb_logger_t *logger, bb_log_level_t level, const char *fmt, ...)
{
    ensure_default_logger_initialized(logger);
    if (!logger || !logger->write || level > logger->level) return;

    va_list args;
    va_start(args, fmt);
    logger->write(logger, level, fmt, args);
    va_end(args);
}

void bb_logger_vlog(bb_logger_t *logger, bb_log_level_t level, const char *fmt, va_list args)
{
    ensure_default_logger_initialized(logger);
    if (!logger || !logger->write || level > logger->level) return;

    logger->write(logger, level, fmt, args);
}

void bb_logger_set_level(bb_logger_t *logger, bb_log_level_t level)
{
    if (!logger) return;
    logger->level = level;
}

void bb_logger_set_mode(bb_logger_t *logger, bb_log_mode_t mode)
{
    if (!logger) return;

    switch (mode)
    {
        case BB_LOG_MODE_QUIET:        logger->level = BB_LOG_LEVEL_ERROR; break;
        case BB_LOG_MODE_NORMAL:       logger->level = BB_LOG_LEVEL_INFO;  break;
        case BB_LOG_MODE_VERBOSE:      logger->level = BB_LOG_LEVEL_DEBUG; break;
        case BB_LOG_MODE_VERY_VERBOSE: logger->level = BB_LOG_LEVEL_TRACE; break;
        default: break;
    }
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
