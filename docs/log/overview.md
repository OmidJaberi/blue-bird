
# Blue-Bird Logging Module Overview

## Introduction

The Blue-Bird logging module provides a lightweight, backend-agnostic logging system built around a pluggable architecture. It supports multiple loggers, allowing different outputs (console, persistent storage, etc.) to coexist in the same application.

The design follows these principles:
- Minimal core API
- Backend extensibility via function pointers
- Per-logger state via `userdata`
- No global logging dependency (except optional `default_logger`)

---

## Architecture

A logger is defined as:

```c
typedef struct bb_logger_t {
    bb_log_level_t level;
    bb_log_write_cb write;
    void *userdata;
} bb_logger_t;
```

Each logger consists of:
- A log level filter
- A backend write function
- Backend-specific state (`userdata`)

---

## Supported Backends

### Console Logger
- Outputs logs to `stdout` or `stderr`
- Stateless aside from FILE pointer

### Persist Logger
- Stores logs using Blue-Bird persist KV API
- Appends log entries under a single key (`bluebird_log`)

---

## Key Features

- Multiple loggers can exist simultaneously
- Backend-agnostic core
- Thread-friendly design (no global state required for operation)
- Optional default logger macros

---

## Logging Flow

1. User calls `BB_LOG_INFO(...)`
2. Macro forwards to `bb_logger_log`
3. Core checks level filtering
4. Backend `write()` function is called
5. Backend handles formatting and output
