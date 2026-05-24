# Blue-Bird Runtime Module

## Overview

The Blue-Bird Runtime module provides the foundational event-driven execution system used by asynchronous infrastructure inside the framework.

The runtime is designed as:

- a lightweight cooperative execution engine
- an event loop abstraction
- a transport-oriented async runtime
- a reusable systems infrastructure layer

The runtime is intentionally minimal and explicit.

The module prioritizes:

- predictability
- composability
- explicit ownership
- modularity
- portability
- incremental evolution

---

# Module Location

```txt
modules/runtime/
```

Public headers:

```txt
include/blue-bird/runtime/
```

---

# Design Goals

The runtime is designed to provide reusable asynchronous infrastructure for:

- networking
- timers
- event-driven systems
- transport layers
- future async subsystems

The runtime intentionally remains:

- low-level
- generic
- embeddable
- transport-oriented

Runtime is the lowest level module does NOT depend on any other module.

This preserves clean dependency direction inside Blue-Bird.

---

# Current Features

The runtime currently supports:

- cooperative task scheduling
- event loop execution
- file descriptor readiness watching
- timer scheduling
- periodic intervals
- one-shot timeouts
- persistent event watchers
- one-shot event watchers
- nonblocking transport integration

---

# Unsupported Features

The runtime intentionally does NOT yet support:

- coroutines
- async/await syntax
- futures/promises
- multithreading
- work stealing
- preemptive scheduling
- thread pools
- io_uring
- epoll/kqueue backends
- cancellation propagation
- structured concurrency

These may be introduced incrementally in future phases.

---

# Public Headers

```c
#include <blue-bird/runtime/runtime.h>
#include <blue-bird/runtime/task.h>
#include <blue-bird/runtime/scheduler.h>
#include <blue-bird/runtime/event.h>
#include <blue-bird/runtime/poller.h>
```

---

# Core Concepts

## Runtime

The runtime is the top-level execution owner.

It coordinates:

- the scheduler
- the event loop
- timers
- file descriptor watchers

Example:

```c
bb_runtime_t *runtime = bb_runtime_create();
```

---

## Task

A task is the smallest executable unit inside the runtime.

Tasks are cooperative and explicitly scheduled.

Example:

```c
typedef void (*bb_task_cb)(
    bb_task_t *task,
    void *userdata
);
```

Tasks are NOT threads.

They run sequentially inside the runtime loop.

---

## Scheduler

The scheduler owns the task queue.

Responsibilities include:

- task insertion
- task ordering
- execution dispatch

The scheduler currently uses a simple FIFO execution model.

---

## Event Loop

The event loop waits for external events.

Current supported events include:

- socket readability
- socket writability
- timers

The loop integrates with the scheduler to wake tasks when events occur.

---

## Watchers

Watchers bind runtime events to tasks.

Example:

```txt
fd readable
    ↓
schedule task
```

Watchers may be:

- persistent
- one-shot

---

## Timers

The runtime supports:

- delayed execution
- periodic intervals

Timers integrate directly into the runtime loop.

---

# Runtime Architecture

The runtime currently follows this high-level architecture:

```txt
runtime
    ↓
event loop
    ↓
poll events
    ↓
schedule tasks
    ↓
execute tasks
```

For networking systems:

```txt
socket readiness
    ↓
runtime watcher
    ↓
task scheduling
    ↓
connection processing
```

---

# Event-Driven Execution Model

The runtime uses:

```txt
cooperative event-driven execution
```

Meaning:

- tasks execute sequentially
- tasks are never preempted
- the runtime controls scheduling
- external events wake execution

This model is similar conceptually to:

- event loops
- transport runtimes
- asynchronous networking systems

---

# Basic Usage

## Minimal Example

```c
#include <stdio.h>

#include <blue-bird/runtime/runtime.h>
#include <blue-bird/runtime/task.h>

static void hello_task(
    bb_task_t *task,
    void *userdata
)
{
    (void) task;
    (void) userdata;

    printf("Hello Runtime\n");
}

int main(void)
{
    bb_runtime_t *runtime =
        bb_runtime_create();

    bb_task_t *task =
        bb_task_create(
            hello_task,
            NULL
        );

    bb_runtime_schedule_task(
        runtime,
        task
    );

    bb_runtime_run(
        runtime
    );

    bb_runtime_destroy(
        runtime
    );

    return 0;
}
```

---

# Timer Example

```c
#include <stdio.h>

#include <blue-bird/runtime/runtime.h>

static void tick(
    bb_task_t *task,
    void *userdata
)
{
    (void) task;
    (void) userdata;

    printf("tick\n");
}

int main(void)
{
    bb_runtime_t *runtime =
        bb_runtime_create();

    bb_runtime_set_interval(
        runtime,
        1000,
        tick,
        NULL
    );

    bb_runtime_run(
        runtime
    );

    return 0;
}
```

---

# File Descriptor Watching

The runtime supports watching file descriptors for readiness events.

Example:

```c
bb_runtime_watch_fd(
    runtime,
    fd,
    BB_EVENT_READ,
    BB_WATCH_PERSISTENT,
    task
);
```

Supported event types:

- `BB_EVENT_READ`
- `BB_EVENT_WRITE`

---

# Watch Modes

## Persistent Watchers

Persistent watchers remain active after triggering.

Used for:

- listening sockets
- recurring events
- long-lived transports

Example:

```c
BB_WATCH_PERSISTENT
```

---

## One-Shot Watchers

One-shot watchers automatically unregister after firing once.

Used for:

- single transport operations
- deferred execution
- transient events

Example:

```c
BB_WATCH_ONESHOT
```

---

# Async Networking Model

The runtime is designed primarily around:

```txt
nonblocking event-driven transport
```

Current architecture supports:

```txt
runtime
    ↓
socket watcher
    ↓
connection task
    ↓
HTTP pipeline
```

This model is currently used by the Blue-Bird async web server.

---

# Connection-Oriented Design

Async transport systems are modeled around:

```txt
connection lifecycle ownership
```

rather than:

```txt
request lifecycle ownership
```

This is important because nonblocking networking requires:

- partial reads
- partial writes
- persistent socket state
- incremental processing

---

# Current Limitations

The current runtime is still considered:

```txt
MVP infrastructure
```

Current limitations include:

- select()-based polling backend
- no coroutine support
- no transport backpressure
- no cancellation model
- no multithreading
- limited scalability
- blocking-style HTTP parsing still exists in some subsystems

These are expected future evolution areas.

---

# Architecture Philosophy

The runtime is intentionally evolving incrementally.

Blue-Bird prioritizes:

- stable abstractions
- composable systems
- explicit ownership
- predictable execution

over:

- aggressive abstraction
- hidden concurrency
- complex scheduling semantics

---

# Recommended Module Layout

```txt
modules/runtime/
├── include/
│   └── blue-bird/runtime/
│       ├── runtime.h
│       ├── scheduler.h
│       ├── task.h
│       ├── event.h
│       └── poller.h
│
└── src/
    ├── runtime.c
    ├── loop.c
    ├── scheduler.c
    ├── task.c
    ├── event.c
    ├── timer.c
    └── poller_select.c
```

---

# Internal Design

The runtime internally combines:

- scheduler
- poller
- watcher registry
- timer registry

The current execution flow is:

```txt
poll events
    ↓
wake tasks
    ↓
run scheduled tasks
    ↓
repeat
```

The scheduler and event loop remain intentionally decoupled.

This allows future evolution toward:

- alternative pollers
- multithreaded runtimes
- hybrid execution models
- coroutine systems

without rewriting the entire runtime architecture.

---

# Future Evolution

Potential future additions include:

- epoll backend
- kqueue backend
- io_uring support
- coroutine integration
- async/await layer
- cancellation propagation
- transport backpressure
- worker pools
- multithreaded runtimes
- hybrid scheduling
- async filesystem support

These features should remain incremental and modular.

---

# Design Philosophy

The runtime is intended to feel like:

- lightweight event runtimes
- transport-oriented async systems
- infrastructure-level execution engines

Not like:

- language VMs
- hidden concurrency systems
- fully managed runtimes

The system intentionally favors:

- explicitness
- composability
- predictable ownership
- incremental abstraction

---

# Summary

The Blue-Bird Runtime module provides:

- cooperative task execution
- event-driven infrastructure
- timer scheduling
- nonblocking transport integration
- runtime-owned execution
- reusable async primitives

while remaining:

- lightweight
- modular
- portable
- explicit
- infrastructure-oriented

