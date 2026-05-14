#ifndef BB_LOG_H
#define BB_LOG_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <stdarg.h>

typedef enum {
    BB_LOG_LEVEL_ERROR = 0,
    BB_LOG_LEVEL_WARN,
    BB_LOG_LEVEL_INFO,
    BB_LOG_LEVEL_DEBUG,
    BB_LOG_LEVEL_TRACE
} bb_log_level_t;

/* Forward declaration */
struct bb_logger_t;

/* Backend write function */
typedef void (*bb_log_write_cb)(struct bb_logger_t *logger, bb_log_level_t level, const char *fmt, va_list args);

/* bb_logger_t struct */
typedef struct bb_logger_t {
    bb_log_level_t level;
    bb_log_write_cb write;   /* Backend-specific write function */
    void *userdata;       /* Backend context (persist store, FILE*, etc.) */
} bb_logger_t;

/* Logging functions */
void bb_logger_log(bb_logger_t *logger, bb_log_level_t level, const char *fmt, ...);
void bb_logger_vlog(bb_logger_t *logger, bb_log_level_t level, const char *fmt, va_list args);

/* Convenience macros (default logger) */
extern bb_logger_t default_logger;

#define BB_LOG_ERROR(fmt, ...) bb_logger_log(&default_logger, BB_LOG_LEVEL_ERROR, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define BB_LOG_WARN(fmt, ...)  bb_logger_log(&default_logger, BB_LOG_LEVEL_WARN,  "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define BB_LOG_INFO(fmt, ...)  bb_logger_log(&default_logger, BB_LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define BB_LOG_DEBUG(fmt, ...) bb_logger_log(&default_logger, BB_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define BB_LOG_TRACE(fmt, ...) bb_logger_log(&default_logger, BB_LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)

/* Close logger (only needed for persist/file backends) */
void bb_logger_close(bb_logger_t *logger);


#ifdef __cplusplus
}
#endif

#endif //BB_LOG_H
