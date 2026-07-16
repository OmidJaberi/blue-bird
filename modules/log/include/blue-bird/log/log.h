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

/* Verbosity presets, so callers don't have to think in terms of levels.
 * Each mode just maps onto a bb_log_level_t under the hood. */
typedef enum {
    BB_LOG_MODE_QUIET = 0,      /* ERROR only                     */
    BB_LOG_MODE_NORMAL,         /* ERROR, WARN, INFO              */
    BB_LOG_MODE_VERBOSE,        /* + DEBUG                        */
    BB_LOG_MODE_VERY_VERBOSE    /* + TRACE                        */
} bb_log_mode_t;

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

/* Runtime verbosity control */
void bb_logger_set_level(bb_logger_t *logger, bb_log_level_t level);
void bb_logger_set_mode(bb_logger_t *logger, bb_log_mode_t mode);

/* Convenience macros (default logger)
 *
 * default_logger is auto-initialized as a console logger at INFO level
 * the first time it's used, so BB_LOG_INFO(...) etc. work out of the box
 * with no bb_logger_init_console() call required. Reassign default_logger
 * (or call bb_logger_init_* on it) before first use to change backends. */
extern bb_logger_t default_logger;

#define BB_LOG_SET_MODE(mode) bb_logger_set_mode(&default_logger, (mode))

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
