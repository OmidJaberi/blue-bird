#ifndef BB_FAULT_INJECT_H
#define BB_FAULT_INJECT_H

/*
 * Failure injection for the web module's allocation and registration paths.
 *
 * How it works: the translation units under test (server.c, async_connection.c,
 * connection.c, conn_list.c, parser.c) are compiled through the thin wrappers
 * in this directory (fi_*.c), which include fi_intercept.h before the real
 * source. That header renames malloc/calloc/realloc/bb_strdup and
 * bb_runtime_watch_fd_ex inside those files only, so every allocation and
 * every runtime registration they make goes through the interposers defined in
 * fault_inject.c. Nothing else in the process is affected, and no linker or
 * loader tricks are needed, so it behaves the same under GCC and AppleClang.
 *
 * Arm a failure with bb_fi_fail_alloc_at(n) / bb_fi_fail_watch_at(n): the n-th
 * intercepted call from that point on fails, exactly once. A test sweeps n
 * from 1 upwards until a run completes without the failure firing, which
 * proves every allocation/registration site on the path was failed in turn.
 * Run under ASan/LSan, a bad error path shows up as a leak or a use-after-free.
 *
 * Single-threaded: the state is global and unsynchronised.
 */

#include <stddef.h>

/* Clear counters and disarm both injectors. */
void bb_fi_reset(void);

/* The n-th intercepted allocation from now on returns NULL (1 = the next one).
 * 0 disarms. The counters restart from zero. */
void bb_fi_fail_alloc_at(unsigned n);

/* The n-th intercepted bb_runtime_watch_fd_ex() from now on returns NULL. */
void bb_fi_fail_watch_at(unsigned n);

unsigned bb_fi_alloc_calls(void);
unsigned bb_fi_alloc_failures(void);
unsigned bb_fi_watch_calls(void);
unsigned bb_fi_watch_failures(void);

/* Interposers; only ever referenced through the fi_intercept.h macros. */
void *bb_fi_malloc(size_t size);
void *bb_fi_calloc(size_t n, size_t size);
void *bb_fi_realloc(void *ptr, size_t size);
char *bb_fi_strdup(const char *s);

#endif /* BB_FAULT_INJECT_H */
