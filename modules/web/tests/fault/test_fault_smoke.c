/*
 * Smoke test for the failure-injection harness itself: proves the wrappers
 * really route the library's allocations and runtime registrations through
 * the injector. If the interposition silently stopped working, every sweep
 * built on it would pass vacuously, so this guards the whole approach.
 */
#include "fault_inject.h"

#include "connection/async_connection.h"
#include "blue-bird/runtime/runtime.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>

int main(void)
{
    printf("Running failure-injection harness smoke test...\n");

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime);

    /* Allocations made inside async_connection.c are counted. */
    bb_fi_reset();
    bb_async_connection_t *conn = bb_async_connection_create(runtime);
    BB_ASSERT(conn);
    BB_ASSERT(bb_fi_alloc_calls() >= 1);
    bb_async_connection_destroy(conn);

    /* ... and the armed one fails, exactly once. */
    bb_fi_fail_alloc_at(1);
    BB_ASSERT(bb_async_connection_create(runtime) == NULL);
    BB_ASSERT(bb_fi_alloc_failures() == 1);
    conn = bb_async_connection_create(runtime);
    BB_ASSERT(conn);
    BB_ASSERT(bb_fi_alloc_failures() == 1);
    bb_async_connection_destroy(conn);

    bb_fi_reset();
    bb_runtime_destroy(runtime);

    printf("Harness smoke test passed.\n");
    return 0;
}
