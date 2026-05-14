#include "blue-bird/log/log.h"
#include "blue-bird/log/console_logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static void console_write(bb_logger_t *logger, bb_log_level_t level, const char *fmt, va_list args)
{
    FILE *out = (FILE*) logger->userdata;
    if (!out) out = stderr;

    /* Timestamp */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

    fprintf(out, "[%s] %-5s: ", timebuf, (level==BB_LOG_LEVEL_ERROR?"ERROR":level==BB_LOG_LEVEL_WARN?"WARN":level==BB_LOG_LEVEL_INFO?"INFO":level==BB_LOG_LEVEL_DEBUG?"DEBUG":"TRACE"));
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    fflush(out);
}

void bb_logger_init_console(bb_logger_t *logger, bb_log_level_t level, FILE *out)
{
    if (!logger) return;
    logger->level = level;
    logger->write = console_write;
    logger->userdata = (void*) out;
}
