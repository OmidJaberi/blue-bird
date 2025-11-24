#include "log/persist_logger.h"
#include "persist/persist.h"
#include "persist/persist_file.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    // currently empty, but allows future expansion
    int dummy;
} PersistLoggerCtx;

static void persist_write(Logger *logger, LogLevel level, const char *fmt, va_list args)
{
    if (!logger || !logger->userdata) return;

    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);

    // timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

    char outbuf[1152];
    snprintf(outbuf, sizeof(outbuf), "[%s] %-5s: %s\n", timebuf,
             (level==LOG_LEVEL_ERROR?"ERROR":
              level==LOG_LEVEL_WARN?"WARN":
              level==LOG_LEVEL_INFO?"INFO":
              level==LOG_LEVEL_DEBUG?"DEBUG":"TRACE"),
             buf);

    // Load existing log
    char prev[65536] = {0}; // adjust if needed
    persist_load("bluebird_log", prev, sizeof(prev));

    // Append new entry
    strncat(prev, outbuf, sizeof(prev)-strlen(prev)-1);

    // Save back to persist
    persist_save("bluebird_log", prev, strlen(prev));
}

void logger_init_persist(Logger *logger, LogLevel level)
{
    if (!logger) return;

    PersistLoggerCtx *ctx = malloc(sizeof(PersistLoggerCtx));
    ctx->dummy = 0;  // placeholder

    logger->level = level;
    logger->write = persist_write;
    logger->userdata = ctx;
}

void logger_free_persist_context(Logger *logger)
{
    if (!logger || !logger->userdata)
        return;
    logger->userdata = NULL;
    logger->write = NULL;
}