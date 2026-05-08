#include "log/log.h"
#include "log/console_logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static void console_write(Logger *logger, LogLevel level, const char *fmt, va_list args)
{
    FILE *out = (FILE*) logger->userdata;
    if (!out) out = stderr;

    /* Timestamp */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

    fprintf(out, "[%s] %-5s: ", timebuf, (level==LOG_LEVEL_ERROR?"ERROR":level==LOG_LEVEL_WARN?"WARN":level==LOG_LEVEL_INFO?"INFO":level==LOG_LEVEL_DEBUG?"DEBUG":"TRACE"));
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    fflush(out);
}

void logger_init_console(Logger *logger, LogLevel level, FILE *out)
{
    if (!logger) return;
    logger->level = level;
    logger->write = console_write;
    logger->userdata = (void*) out;
}
