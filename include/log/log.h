#ifndef BLUE_BIRD_LOG_H
#define BLUE_BIRD_LOG_H

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} LogLevel;

/* Forward declaration */
struct Logger;

/* Backend write function */
typedef void (*LogWriteFunc)(struct Logger *logger, LogLevel level, const char *fmt, va_list args);

/* Logger struct */
typedef struct Logger {
    LogLevel level;
    LogWriteFunc write;   /* Backend-specific write function */
    void *userdata;       /* Backend context (persist store, FILE*, etc.) */
} Logger;

/* Logging functions */
void logger_log(Logger *logger, LogLevel level, const char *fmt, ...);
void logger_vlog(Logger *logger, LogLevel level, const char *fmt, va_list args);

/* Convenience macros (default logger) */
extern Logger default_logger;

#define LOG_ERROR(fmt, ...) logger_log(&default_logger, LOG_LEVEL_ERROR, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  logger_log(&default_logger, LOG_LEVEL_WARN,  "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  logger_log(&default_logger, LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) logger_log(&default_logger, LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) logger_log(&default_logger, LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

/* Close logger (only needed for persist/file backends) */
void logger_close(Logger *logger);

#endif /* BLUE_BIRD_LOG_H */
