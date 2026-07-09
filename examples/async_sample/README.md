# Blue-Bird Async Sample

A terminal app with no web server at all — it exists to show Blue-Bird's
`runtime` module on its own: the same event loop that `bb_server_start`
runs on top of, driving a repeating timer and a raw-mode stdin watcher side
by side. Useful for understanding what's actually running underneath every
other example before you add HTTP into the mix.

## Running

From the repository root:

```bash
mkdir build && cd build
cmake ..
make example_async
./examples/async_sample/example_async
```

A `counter: N` line prints once a second. Press `q` to stop the runtime and
exit cleanly.

## Layout

```
examples/async_sample/
├── CMakeLists.txt   (only links bluebird_runtime — no bluebird_web)
└── main.c
```

## What it shows

Blue-Bird's runtime is a single-threaded event loop (`bb_runtime_t`) that
schedules `bb_task_t` callbacks, either on a timer or in response to file
descriptor activity. This example drives two independent tasks on one
runtime:

**1. A repeating timer task**, incrementing and printing a counter every
second:

```c
bb_task_t *counter = bb_task_create(counter_task, &app);
bb_runtime_set_interval(runtime, 1000, counter);
```

**2. A persistent stdin watcher**, woken up whenever a byte is available to
read (terminal is put into raw mode first via `termios` so keypresses
arrive without waiting for Enter):

```c
bb_task_t *stdin_watcher = bb_task_create(stdin_task, runtime);
bb_runtime_watch_fd(
    runtime,
    STDIN_FILENO,
    BB_EVENT_READ,
    BB_WATCH_PERSISTENT,
    stdin_watcher
);
```

`stdin_task` reads one character; on `'q'` it restores the terminal and
calls `bb_runtime_stop(runtime)`, which unblocks `bb_runtime_run`:

```c
bb_runtime_run(runtime);      // blocks here, running both tasks, until stopped
bb_task_destroy(counter);
bb_runtime_destroy(runtime);
```

Every `bb_task_t` carries an opaque `void *userdata` pointer, which is how
both callbacks get access to shared state (`app_state_t` for the counter,
the `bb_runtime_t *` itself for the stdin watcher) without any globals.

## Relation to the web examples

`hello`, `todo`, and `chat` all call `bb_runtime_run_default()` at the end
of `main()` — that's this exact loop, just pre-created and driven by
`bb_server_start()` registering the listening socket as another watched fd
instead of `STDIN_FILENO`. If you're debugging why a web example "hangs" on
`bb_runtime_run_default()`, this is what's actually looping underneath.
