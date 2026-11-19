# Failure-injection tests

These tests fail allocations and runtime registrations on purpose and check
that the web module reports the failure, stays usable, and releases everything
it had acquired.

## How failures are injected

`server.c`, `connection/async_connection.c`, `connection/connection.c`,
`connection/conn_list.c` and `http/parser.c` are compiled a second time into
each test executable, through the one-line wrappers `fi_*.c`. Each wrapper
includes `fi_intercept.h` first, which renames `malloc`, `calloc`, `realloc`,
`bb_strdup` and `bb_runtime_watch_fd_ex` *inside that file only* to the
interposers in `fault_inject.c`. The test executable then defines every symbol
of those files itself, so the linker never takes the regular copies from
`libbluebird_web.a`.

There is no `ld --wrap`, `LD_PRELOAD` or allocator replacement involved, so it
behaves the same under GCC and AppleClang and alongside ASan.

Only allocations and registrations made by those five files are intercepted.
Code outside them (the HTTP message/response objects, the runtime itself, the
test's own code) always runs against the real allocator.

## Sweeps

`bb_fi_fail_alloc_at(n)` makes the n-th intercepted allocation from that point
fail, once. A test sweeps `n = 1, 2, 3, ...` and stops at the first run where
the failure never fires, which means every allocation on the path has been
failed in turn. `bb_fi_fail_watch_at(n)` does the same for runtime
registrations (`bb_runtime_watch_fd_ex`).

After each failed run the tests check that the operation reported the
failure, that the object is unchanged (so the call can be retried), that a
fresh request still works, and that no descriptor leaked. Leaks and
use-after-free in error paths are caught by running the suite under
`-DBB_ENABLE_SANITIZERS=ON`; the report points at the allocation that was
leaked. Each sweep also asserts a minimum number of failure points, so a
broken interposition cannot make it pass vacuously (`test_fault_smoke.c`
guards the interposition itself).

## Adding a test

Add `fault/test_fault_<name>.c` and register it in `tests/CMakeLists.txt` with
`bb_add_web_fault_test(test_fault_<name>)`. If the path under test lives in a
file that is not wrapped yet, add a `fi_<file>.c` wrapper and list it in
`BB_WEB_FAULT_SOURCES`.
