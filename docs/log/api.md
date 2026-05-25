
# Blue-Bird Logging API Reference

## Core Types

### Log Level

```c
typedef enum {
    BB_LOG_LEVEL_ERROR = 0,
    BB_LOG_LEVEL_WARN,
    BB_LOG_LEVEL_INFO,
    BB_LOG_LEVEL_DEBUG,
    BB_LOG_LEVEL_TRACE
} bb_log_level_t;
```

---

### Logger Struct

```c
typedef struct bb_logger_t {
    bb_log_level_t level;
    bb_log_write_cb write;
    void *userdata;
} bb_logger_t;
```

---

## Core Functions

### bb_logger_log

```c
void bb_logger_log(bb_logger_t *logger,
                   bb_log_level_t level,
                   const char *fmt, ...);
```

Logs a formatted message.

---

### bb_logger_vlog

```c
void bb_logger_vlog(bb_logger_t *logger,
                    bb_log_level_t level,
                    const char *fmt,
                    va_list args);
```

Lower-level variant used by backend implementations.

---

### bb_logger_close

```c
void bb_logger_close(bb_logger_t *logger);
```

Closes a logger instance.  
Note: does not free backend memory.

---

## Default Logger

```c
extern bb_logger_t default_logger;
```

Used by macros:

```c
BB_LOG_INFO("Hello");
BB_LOG_ERROR("Error: %d", code);
```

---

## Macros

- BB_LOG_ERROR
- BB_LOG_WARN
- BB_LOG_INFO
- BB_LOG_DEBUG
- BB_LOG_TRACE
