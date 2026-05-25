
# Blue-Bird Logging Backends

## Console Logger

### Initialization

```c
void bb_logger_init_console(bb_logger_t *logger,
                            bb_log_level_t level,
                            FILE *out);
```

### Behavior

- Writes logs to FILE stream
- Formats timestamp + level + message
- Flushes output immediately

### userdata

Stores:
```c
FILE *out;
```

---

## Persist Logger

### Initialization

```c
void bb_logger_init_persist(bb_logger_t *logger,
                            bb_log_level_t level);
```

---

### Behavior

The persist logger:
- Loads existing log from key: `bluebird_log`
- Appends new log entry
- Saves back using KV persist API

### Storage Model

All logs are stored under a single key:

```
bluebird_log
```

This ensures:
- Simple retrieval
- Backend independence
- Append-style logging

---

### Limitations

- Entire log is rewritten on each entry
- Not suitable for extremely large logs without rotation
- Performance depends on persist backend implementation

---

### Context

```c
typedef struct {
    int dummy;
} _bb_persist_logger_ctx;
```

Currently minimal but allows future expansion such as:
- rotation counters
- batching
- compression
