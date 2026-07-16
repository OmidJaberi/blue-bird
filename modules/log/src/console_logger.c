#include "blue-bird/log/log.h"
#include "blue-bird/log/console_logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#if defined(_WIN32)
    #include <io.h>
    #define BB_ISATTY _isatty
    #define BB_FILENO _fileno
#else
    #include <unistd.h>
    #define BB_ISATTY isatty
    #define BB_FILENO fileno
#endif

#define BB_COLOR_RED    "\033[31m"
#define BB_COLOR_YELLOW "\033[33m"
#define BB_COLOR_GREEN  "\033[32m"
#define BB_COLOR_CYAN   "\033[36m"
#define BB_COLOR_GRAY   "\033[90m"
#define BB_COLOR_RESET  "\033[0m"

static const char *level_name(bb_log_level_t level)
{
    switch (level)
    {
        case BB_LOG_LEVEL_ERROR: return "ERROR";
        case BB_LOG_LEVEL_WARN:  return "WARN";
        case BB_LOG_LEVEL_INFO:  return "INFO";
        case BB_LOG_LEVEL_DEBUG: return "DEBUG";
        case BB_LOG_LEVEL_TRACE: return "TRACE";
        default:                 return "?????";
    }
}

static const char *level_color(bb_log_level_t level)
{
    switch (level)
    {
        case BB_LOG_LEVEL_ERROR: return BB_COLOR_RED;
        case BB_LOG_LEVEL_WARN:  return BB_COLOR_YELLOW;
        case BB_LOG_LEVEL_INFO:  return BB_COLOR_GREEN;
        case BB_LOG_LEVEL_DEBUG: return BB_COLOR_CYAN;
        case BB_LOG_LEVEL_TRACE: return BB_COLOR_GRAY;
        default:                 return "";
    }
}

static void console_write(bb_logger_t *logger, bb_log_level_t level, const char *fmt, va_list args)
{
    FILE *out = (FILE*) logger->userdata;
    /* No explicit stream given: split by severity so ERROR/WARN show up
     * on stderr (visible even if stdout is redirected/piped) while
     * everything else goes to stdout. */
    if (!out) out = (level <= BB_LOG_LEVEL_WARN) ? stderr : stdout;

    /* Only emit color escapes when the destination is an actual terminal,
     * so redirecting to a file or piping through another tool doesn't
     * leave escape codes littered in the output. */
    int use_color = BB_ISATTY(BB_FILENO(out));

    /* Timestamp */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

    if (use_color)
        fprintf(out, "[%s] %s%-5s%s: ", timebuf, level_color(level), level_name(level), BB_COLOR_RESET);
    else
        fprintf(out, "[%s] %-5s: ", timebuf, level_name(level));

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
